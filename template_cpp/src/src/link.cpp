#include "link.hpp"

/* =============================== Port Implementation =============================== */
Port::Port(int socket, sockaddr_in source_addr, sockaddr_in dest_addr) 
  : socket(socket), source_addr(source_addr), dest_addr(dest_addr) {}

SenderPort::SenderPort(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : Port(socket, source_addr, dest_addr), message_queue(), pending_msgs(true)
{}

void SenderPort::enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq)
{  
  // Create message
  link_seq++;
  std::tuple<msg_seq_t, proc_id_t, msg_seq_t> messageTuple = std::make_tuple(link_seq, origin_id, m_seq);
  
  // Append message to end of message queue
  message_queue.push_back(messageTuple); 
}

void SenderPort::send()
{
  // No messages to send
  if (pending_msgs.empty() && message_queue.empty()) return;

  // Complete pending_msgs set with messages from message_queue and get snapshot of new pending_msgs set
  std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> setSnapshot = pending_msgs.complete(message_queue);
  auto it = setSnapshot.begin();

  // std::cout << "message_queue size: " << message_queue.size() << ", pending_messages size: " << pending_msgs.size() << std::endl;
  
  for (size_t i = 0; i < window_size; i++) {
    std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> msgs;

    for (size_t i = 0; i < Message::max_msgs && it != setSnapshot.end(); i++, it++) {
      msgs.push_back(*it);
    }
    
    assert(msgs.size() <= 8);
    Message message(MES, static_cast<uint8_t>(msgs.size()), msgs);
    // Message::displayMessage(message);
    // Message::displaySerialized(message.serialize());
    // std::cout << std::endl;
  
    // Send message to receiver
    if (sendto(socket, message.serialize(), message.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send message " << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      std::cout << os.str() << "\n";
      continue;
      // throw std::runtime_error(os.str());
    }

    // std::cout << "Message sent successfully." << std::endl;

    // Terminate if all messages have been sent
    if (it == setSnapshot.end()) {
      // std::cout << "All messages in queue sent." << std::endl;
      break;
    }
  }
}

void SenderPort::receiveAck(std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> acked_messages)
{
  // Remove messages acknowledged by receiver
  pending_msgs.erase(acked_messages);
}

ReceiverPort::ReceiverPort(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : Port(socket, source_addr, dest_addr), deliveredMessages() {}

std::vector<bool> ReceiverPort::respond(Message message)
{
  // deliveredMessages.display();

  // Update delivered message set and construct delivery status vector
  std::vector<bool> delivery_status = deliveredMessages.insert(message.getSeqs());

  // Transform message to ack and serialize
  Message ackMsg = message.toAck();

  // Send ACK to sender
  if (sendto(socket, ackMsg.serialize(), ackMsg.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
    std::ostringstream os;
    os << "Failed to send ACK (errno: " << strerror(errno) << ") from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
    std::cout << os.str() << "\n";
    return {};
    // throw std::runtime_error(os.str());
  }
  return delivery_status;
}

/* =============================== Link Implementation =============================== */
PerfectLink::PerfectLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : sendPort(socket, source_addr, dest_addr), recvPort(socket, source_addr, dest_addr) {}

void PerfectLink::enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq)
{
  sendPort.enqueueMessage(origin_id, m_seq);
}

void PerfectLink::send()
{
  return sendPort.send();
}

std::vector<bool> PerfectLink::receive(Message mes)
{
  MessageType type = mes.getType();

  if (mes.getType() == MES) { 
    // Deliver message to receiver link
    return recvPort.respond(mes);
  }
  else if (type == ACK) {
    // Process ACK on sender link
    sendPort.receiveAck(mes.getPayloads());
    // std::cout << "Processed ACK for messages: ";
    for (msg_seq_t seq: mes.getSeqs()) {
      // std::cout << seq << " ";
      }
    // std::cout << "" << std::endl;
    return {};
  }
  else {
    throw std::runtime_error("Unknown message type received.");
  }
}

