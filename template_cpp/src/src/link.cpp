#include "link.hpp"

/* =============================== Port Implementation =============================== */
Port::Port(int socket, proc_id_t source_id, sockaddr_in source_addr, sockaddr_in dest_addr) 
  : socket(socket), source_id(source_id), source_addr(source_addr), dest_addr(dest_addr) {}

SenderPort::SenderPort(int socket, proc_id_t source_id, sockaddr_in source_addr, sockaddr_in dest_addr)
  : Port(socket, source_id, source_addr, dest_addr)
{
  messageQueue = {};
}

void SenderPort::enqueueMessage(msg_seq_t m_seq)
{  
  // Wait until there is space in the queue
  std::unique_lock<std::mutex> lock(queue_mutex);
  queue_cv.wait(lock, [&]() {
    return messageQueue.size() < maxQueueSize;
  });
  
  // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
  messageQueue.insert(messageQueue.end(), m_seq); 
  
  // Notify any waiting threads that a new message has been enqueued
  lock.unlock();
}

void SenderPort::send()
{
  // Lock the queue while accessing it
  std::lock_guard<std::mutex> lock(queue_mutex);

  if (messageQueue.empty()) {
    return; // No messages to send
  }

  // set up for message iterator and send failure array
  std::set<msg_seq_t>::iterator it = messageQueue.begin();
  
  for (size_t i = 0; i < window_size; i++) {
    std::vector<msg_seq_t> msgs;

    uint8_t nb_msgs = 0;
    for (nb_msgs = 0; nb_msgs < Message::max_msgs && it != messageQueue.end(); nb_msgs++, it++) {
      msgs.push_back(*it);
    }
    
    // Create message to send and display
    std::cout << "\nMessage to send:\n";
    Message message(MES, source_id, nb_msgs, msgs);
    Message::displayMessage(message);
    Message::displaySerialized(message.serialize());
    std::cout << std::endl;

    std::cout << "Sending message with seq numbers: ";
    for (msg_seq_t seq: msgs) {
      std::cout << seq << " ";
    }
    std::cout << "\n";
  
    // Send message to receiver
    if (sendto(socket, message.serialize(), message.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send message " << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      throw std::runtime_error(os.str());
    }

    std::cout << "Message sent successfully.\n";

    // Terminate if all messages have been sent
    if (it == messageQueue.end()) {
      std::cout << "All messages in queue sent.\n";
      break;
    }
  }
}

void SenderPort::receiveAck(std::vector<msg_seq_t> acked_messages)
{
  // Lock the queue while modifying it
  std::unique_lock<std::mutex> lock(queue_mutex);
  
  // Remove message from queue (correctly delivered at target)
  for (msg_seq_t seq: acked_messages) {
    messageQueue.erase(seq);
  }
  
  // Notify the waiting enqueuer thread that space is available in the queue
  lock.unlock();
  queue_cv.notify_one();
  
  all_msg_delivered = messageQueue.empty();
  // Notify finished condition variable to maybe signal completion to main thread
  finished_cv.notify_one();
}

void SenderPort::allMessagesEnqueued() {
  std::lock_guard<std::mutex> lock(finished_mutex);
  all_msg_enqueued = true;
}

void SenderPort::finished() {
  std::unique_lock<std::mutex> lock(finished_mutex);
  finished_cv.wait(lock, [&]() {
    return all_msg_enqueued && all_msg_delivered;
  });

  // Unlock and return to signal main thread of completion 
  lock.unlock();
  return;
}

ReceiverPort::ReceiverPort(int socket, proc_id_t source_id, sockaddr_in source_addr, sockaddr_in dest_addr)
  : Port(socket, source_id, source_addr, dest_addr), deliveredMessages({0}) {}

std::vector<bool> ReceiverPort::respond(Message message)
{
  std::cout << "Delivered messages set: ";
  for (msg_seq_t seq: deliveredMessages) {
    std::cout << seq << " ";
  }
  std::cout << "\n";

  // Update delivered message set and construct delivery status vector
  std::vector<bool> delivery_status;
  std::set<msg_seq_t>::iterator it = deliveredMessages.begin();
  for (msg_seq_t m_seq: message.getSeqs()) {
    if (m_seq < *it) {
      delivery_status.push_back(false); // already delivered
      std::cout << "Received message with seq number: " << m_seq << " delivered: " << false << ". Delivered set size: " << deliveredMessages.size() << "\n";
    }
    else {
      auto result = deliveredMessages.insert(m_seq);
      delivery_status.push_back(result.second); // true if inserted (not delivered before), false otherwise
      std::cout << "Received message with seq number: " << m_seq << " delivered: " << result.second << ". Delivered set size: " << deliveredMessages.size() << "\n";
    }
  }

  // Remove leading delivered messages to avoid unbounded growth
  while (it != deliveredMessages.end()) {
    if (*it + 1 != *(++it)) {
      break;
    }
  }
  deliveredMessages.erase(deliveredMessages.begin(), --it);

  // Transform message to ack and serialize
  Message ackMsg = message.toAck();

  // Send ACK to sender
  if (sendto(socket, ackMsg.serialize(), ackMsg.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
    std::ostringstream os;
    os << "Failed to send ACK (errno: " << strerror(errno) << ") from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
    throw std::runtime_error(os.str());
  }
  return delivery_status;
}

/* =============================== Link Implementation =============================== */
PerfectLink::PerfectLink(int socket, proc_id_t source_id, sockaddr_in source_addr, sockaddr_in dest_addr)
  : sendPort(socket, source_id, source_addr, dest_addr), recvPort(socket, source_id, source_addr, dest_addr) {}

void PerfectLink::enqueueMessage(msg_seq_t m_seq)
{
  sendPort.enqueueMessage(m_seq);
}

void PerfectLink::send()
{
  return sendPort.send();
}

std::vector<bool> PerfectLink::receive(Message mes)
{
  proc_id_t sender_id = mes.getOriginId();
  MessageType type = mes.getType();
  std::vector<msg_seq_t> messages = mes.getSeqs();

  if (mes.getType() == MES) { 
    // Deliver message to receiver link
    std::vector<bool> received = recvPort.respond(mes);
    return received;
  }
  else if (type == ACK) {
    // Process ACK on sender link
    sendPort.receiveAck(messages);
    std::cout << "Processed ACK for messages: ";
    for (msg_seq_t m_seq: messages) {
      std::cout << m_seq << " ";
      }
    std::cout << "\n";
    return {};
  }
  else {
    throw std::runtime_error("Unknown message type received.");
  }
}

void PerfectLink::allMessagesEnqueued()
{
  sendPort.allMessagesEnqueued();
}

void PerfectLink::finished()
{
  sendPort.finished();
}

