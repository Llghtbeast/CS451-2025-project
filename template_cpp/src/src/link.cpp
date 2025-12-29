#include "link.hpp"

PerfectLink::PerfectLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : socket(socket), source_addr(source_addr), dest_addr(dest_addr), 
    packet_queue(), pending_pkts(true), delivered_pkts()
{}

void PerfectLink::enqueueMessage(Message msg)
{
  // Create message
  link_seq++;
  std::pair<pkt_seq_t, Message> messageTuple = std::make_pair(link_seq, msg);
  
  // Append message to end of message queue
  packet_queue.push_back(messageTuple); 
}

void PerfectLink::send()
{
  // No packets to send
  if (pending_pkts.empty() && packet_queue.empty()) return;

  // Complete pending_pkts set with messages from packet_queue and get snapshot of new pending_pkts set
  const auto& [setSnapshot, size] = pending_pkts.complete(packet_queue);
  size_t it = 0;

  // std::cout << "packet_queue size: " << packet_queue.size() << ", pending_messages size: " << pending_pkts.size() << std::endl;
  for (size_t i = 0; i < window_size; i++) {
    std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;
    std::array<Message, MAX_MESSAGES_PER_PACKET> msgs;

    uint8_t count = 0;
    for (; count < Packet::max_msgs && it < size; count++, it++) {
      seqs[count] = setSnapshot[it].first;
      msgs[count] = setSnapshot[it].second;
    }
    
    Packet packet(MES, count, seqs, msgs);
    // packet.displayPacket();
    // Packet::displaySerialized(packet.serialize());
    // std::cout << std::endl;
  
    // Send packet to receiver
    if (sendto(socket, packet.serialize(), packet.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send packet (errno: " << strerror(errno) << ")from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      std::cout << os.str() << "\n";
      continue;
      // throw std::runtime_error(os.str());
    }

    // std::cout << "Packet sent successfully." << std::endl;

    // Terminate if all messages have been sent
    if (it == size) {
      // std::cout << "All messages in queue sent." << std::endl;
      break;
    }
  }
}

std::array<bool, MAX_MESSAGES_PER_PACKET> PerfectLink::receive(Packet packet)
{
  MessageType type = packet.getType();

  if (packet.getType() == MES) { 
    // Update delivered message set and construct delivery status vector
    std::array<bool, MAX_MESSAGES_PER_PACKET> delivery_status = delivered_pkts.insert(packet.getSeqs(), packet.getNbMes());

    // Transform message to ack and serialize
    Packet ack_pkt = packet.toAck();

    // Send ACK to sender
    if (sendto(socket, ack_pkt.serialize(), ack_pkt.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
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
    pending_pkts.erase(packet.getSeqs());
    return {};
  }
  else {
    throw std::runtime_error("Unknown message type received.");
  }
}

