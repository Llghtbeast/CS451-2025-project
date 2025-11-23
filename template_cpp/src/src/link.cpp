#include "link.hpp"

/* =============================== Port Implementation =============================== */
/**
 * Constructor to initialize the Port with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
Port::Port(int socket, unsigned long source_id, sockaddr_in source_addr, unsigned long dest_id, sockaddr_in dest_addr) 
  : socket(socket), source_id(source_id), source_addr(source_addr), dest_id(dest_id), dest_addr(dest_addr) {}

/**
 * Constructor to initialize the SenderPort with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
SenderPort::SenderPort(int socket, unsigned long source_id, sockaddr_in source_addr, unsigned long dest_id, sockaddr_in dest_addr)
  : Port(socket, source_id, source_addr, dest_id, dest_addr)
{
  messageQueue = {};
}

/**
* Enqueues a message to be sent later.
* @param m_seq The sequence number of the message.
*/
uint32_t SenderPort::enqueueMessage()
{  
  // Increment message sequence number
  m_seq++;

  // Wait until there is space in the queue
  std::unique_lock<std::mutex> lock(queue_mutex);
  queue_cv.wait(lock, [&]() {
    return messageQueue.size() < maxQueueSize;
  });
  
  // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
  messageQueue.insert(messageQueue.end(), m_seq); 
  
  // Notify any waiting threads that a new message has been enqueued
  lock.unlock();

  return m_seq;
}

/**
 * Send the first enqueued messages.
 * @throws std::runtime_error if sending fails.
 */
void SenderPort::send()
{
  // Lock the queue while accessing it
  std::lock_guard<std::mutex> lock(queue_mutex);

  if (messageQueue.empty()) {
    return; // No messages to send
  }

  // set up for message iterator and send failure array
  std::set<uint32_t>::iterator it = messageQueue.begin();
  
  for (uint32_t i = 0; i < window_size; i++) {
    std::vector<uint32_t> msgs;

    uint8_t nb_msgs = 0;
    for (nb_msgs = 0; nb_msgs < Message::max_msgs && it != messageQueue.end(); nb_msgs++, it++) {
      msgs.push_back(*it);
    }

    // Transform message sequence number into big-endian for network transport
    Message message(MES, source_id, nb_msgs, msgs);
    std::cout << "Sending message with seq numbers: ";
    for (uint32_t seq: msgs) {
      std::cout << seq << " ";
    }
    std::cout << "\n";
  
    // Send message to receiver
    if (sendto(socket, message.serialize(), message.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send message " << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      throw std::runtime_error(os.str());
    }

    // Terminate if all messages have been sent
    if (it == messageQueue.end()) {
      break;
    }
  }
}

/** 
 * Receive ACK from receiver and removed corresponding message from queue.
 * @param m_seq The sequence number of the acknowledged message.
*/
void SenderPort::receiveAck(std::vector<uint32_t> acked_messages)
{
  // Lock the queue while modifying it
  std::unique_lock<std::mutex> lock(queue_mutex);
  
  // Remove message from queue (correctly delivered at target)
  for (uint32_t seq: acked_messages) {
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

/**
 * Constructor to initialize the ReceiverPort with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
ReceiverPort::ReceiverPort(int socket, unsigned long source_id, sockaddr_in source_addr, unsigned long dest_id, sockaddr_in dest_addr)
  : Port(socket, source_id, source_addr, dest_id, dest_addr), deliveredMessages({0}) {}

/**
 * Add message to delivered list, send ACK to sender, and return true if message was not already delivered. Otherwise, return false.
 * @param m_seq The sequence number of the message to be delivered.
 * @return True if the message was not already delivered, false otherwise.
 */
std::vector<bool> ReceiverPort::respond(Message message)
{
  std::cout << "Delivered messages set: ";
  for (uint32_t seq: deliveredMessages) {
    std::cout << seq << " ";
  }
  std::cout << "\n";

  // Update delivered message set and construct delivery status vector
  std::vector<bool> delivery_status;
  std::set<uint32_t>::iterator it = deliveredMessages.begin();
  for (uint32_t m_seq: message.getSeqs()) {
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
/**
 * Constructor to initialize the PerfectLink with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
PerfectLink::PerfectLink(int socket, unsigned long source_id, sockaddr_in source_addr, unsigned long dest_id, sockaddr_in dest_addr)
  : sendPort(socket, source_id, source_addr, dest_id, dest_addr), recvPort(socket, source_id, source_addr, dest_id, dest_addr) {}


uint32_t PerfectLink::enqueueMessage()
{
  return sendPort.enqueueMessage();
}

void PerfectLink::send()
{
  return sendPort.send();
}

void PerfectLink::receive(Message mes, Logger &logger)
{
  unsigned long sender_id = mes.getOriginId();
  MessageType type = mes.getType();
  std::vector<uint32_t> messages = mes.getSeqs();

  if (mes.getType() == MES) {
    // Deliver message to receiver link
    std::vector<bool> received = recvPort.respond(mes);
    for (size_t i = 0; i < messages.size(); i++) {
      uint32_t m_seq = messages[i];
      bool was_received = received[i];
      if (was_received) {
        std::cout << "d " << sender_id << " " << m_seq << "\n";
        logger.logDelivery(sender_id, m_seq);
      }
    }
  }
  else if (type == ACK) {
    // Process ACK on sender link
    sendPort.receiveAck(messages);
    std::cout << "Processed ACK for messages: ";
    for (uint32_t m_seq: messages) {
      std::cout << m_seq << " ";
      }
    std::cout << "\n";
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

