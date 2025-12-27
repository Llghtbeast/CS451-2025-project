#include "link.hpp"

PerfectLink::PerfectLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : socket(socket), source_addr(source_addr), dest_addr(dest_addr), 
    message_queue(), pending_msgs(true), deliveredPackets()
{}

void PerfectLink::enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq)
{
  // Create message
  link_seq++;
  std::tuple<msg_seq_t, proc_id_t, msg_seq_t> messageTuple = std::make_tuple(link_seq, origin_id, m_seq);
  
  // Append message to end of message queue
  message_queue.push_back(messageTuple); 
}

void PerfectLink::send()
{
  // No packets to send
  if (pending_msgs.empty() && message_queue.empty()) return;

  // Complete pending_msgs set with messages from message_queue and get snapshot of new pending_msgs set
  std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> setSnapshot = pending_msgs.complete(message_queue);
  auto it = setSnapshot.begin();

  // std::cout << "message_queue size: " << message_queue.size() << ", pending_messages size: " << pending_msgs.size() << std::endl;
  
  for (size_t i = 0; i < window_size; i++) {
    std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> msgs;

    for (size_t i = 0; i < Packet::max_msgs && it != setSnapshot.end(); i++, it++) {
      msgs.push_back(*it);
    }
    
    assert(msgs.size() <= 8);
    Packet packet(MES, static_cast<uint8_t>(msgs.size()), msgs);
    // Packet::displayPacket(packet);
    // Packet::displaySerialized(packet.serialize());
    // std::cout << std::endl;
  
    // Send packet to receiver
    if (sendto(socket, packet.serialize(), packet.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send packet " << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      std::cout << os.str() << "\n";
      continue;
      // throw std::runtime_error(os.str());
    }

    // std::cout << "Packet sent successfully." << std::endl;

    // Terminate if all messages have been sent
    if (it == setSnapshot.end()) {
      // std::cout << "All messages in queue sent." << std::endl;
      break;
    }
  }
}

std::vector<bool> PerfectLink::receive(Packet packet)
{
  MessageType type = packet.getType();

  if (packet.getType() == MES) { 
    // Update delivered message set and construct delivery status vector
    std::vector<bool> delivery_status = deliveredPackets.insert(packet.getSeqs());

    // Transform message to ack and serialize
    Packet ackMsg = packet.toAck();

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
  else if (type == ACK) {
    // Remove messages acknowledged by receiver
    pending_msgs.erase(packet.getPayloads());
    return {};
  }
  else {
    throw std::runtime_error("Unknown message type received.");
  }
}

