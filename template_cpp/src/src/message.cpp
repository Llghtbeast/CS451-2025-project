#include "message.hpp"

// =================== Message implementation =================== 
Message::Message(msg_seq_t seq, proc_id_t origin): seq(seq), origin(origin) {}

bool Message::operator==(const Message& other) const {
    return (seq == other.seq) && (origin == other.origin);
}

void Message::displayMessage(const Message& msg)
{
  std::cout << "Message: ";
  std::cout << msg.seq << " ";
  std::cout << msg.origin << "\n";
}

void Message::serializeTo(std::vector<char>& buffer) const 
{
  msg_seq_t seq_network = convertToNetwork(seq);
  proc_id_t orig_network = convertToNetwork(origin);
  
  const char* p1 = reinterpret_cast<const char*>(&seq_network);
  buffer.insert(buffer.end(), p1, p1 + sizeof(seq_network));
  
  const char* p2 = reinterpret_cast<const char*>(&orig_network);
  buffer.insert(buffer.end(), p2, p2 + sizeof(orig_network));
}

Message Message::deserialize(const char *buffer, size_t& offset)
{
  msg_seq_t seq_network;
  proc_id_t orig_network;
  
  std::memcpy(&seq_network, buffer + offset, sizeof(seq_network));
  offset += sizeof(msg_seq_t);
  
  std::memcpy(&orig_network, buffer + offset, sizeof(orig_network));
  offset += sizeof(proc_id_t);
  
  return Message(convertFromNetwork(seq_network), convertFromNetwork(orig_network));
}

// =================== Packet implementation =================== 
// Constructor for MES
Packet::Packet(MessageType type, uint8_t nb_m, 
                 std::vector<std::pair<pkt_seq_t, Message>> payloads)
    : m_type(type), origin_id(0), nb_mes(nb_m), payload(std::move(payloads)) 
{
  assert(nb_m == payload.size());
}

// Packet::Packet(MessageType type, uint8_t nb_m, std::vector<msg_seq_t> seqs)
//     : m_type(type), nb_mes(nb_m), payload(std::move(seqs)) 
// {
//   assert(type == MessageType::ACK);
//   assert(nb_m == seqs.size());
// }

MessageType Packet::getType() const  { return m_type; }
uint8_t Packet::getNbMes() const     { return nb_mes; }

const std::vector<std::pair<pkt_seq_t, Message>>& Packet::getPayloads() const 
{
  return payload;
  // assert(m_type == MessageType::MES);
  // return std::get<0>(payload);
}

const std::vector<msg_seq_t> Packet::getSeqs() const
{ 
  std::vector<msg_seq_t> seqs;
  seqs.reserve(nb_mes);
  for (const auto& [seq, _] : payload) {
    seqs.push_back(seq);
  }
  return seqs; 
  // assert(m_type == MessageType::ACK);
  // return std::get<1>(payload);
}

/** 
 * Convert message to ACK type
 */
Packet Packet::toAck()
{
  // // Extract link sequence numbers from MES payload
  // auto payloads = std::get<0>(payload);
  // std::vector<msg_seq_t> seqs;
  // seqs.reserve(nb_mes);
  // for (const auto& tup : payloads) {
  //   // extract link sequence number
  //   seqs.push_back(std::get<0>(tup)); 
  // }
  
  // Construct ACK message
  Packet new_msg = Packet(ACK, nb_mes, payload);
  return new_msg;
}

size_t Packet::serializedSize() const 
{
  size_t size = sizeof(m_type) + sizeof(nb_mes);

  // if (m_type == MES) {
  size += nb_mes * (sizeof(pkt_seq_t) + Message::serializedSize);
  // } else if (m_type == ACK) {
  //   size += nb_mes * sizeof(msg_seq_t);
  // }

  return size;
}

void Packet::displayPacket(const Packet& message)
{
  std::cout << "Packet Type: " << static_cast<int>(message.getType()) << "" << std::endl;
  std::cout << "Number of Packets: " << static_cast<int>(message.getNbMes()) << "" << std::endl;
  std::cout << "Payload:\n";
  for (const auto& [seq, msg] : message.getPayloads()) {
    std::cout << "    ";
    std::cout << seq<< " ";
    Message::displayMessage(msg);
  }
  std::cout << "" << std::endl;
}

void Packet::displaySerialized(const char* serialized)
{
  size_t len = Packet::deserialize(serialized).serializedSize();
  std::cout << "Serialized message size: " << len << std::endl;
  std::cout << "Serialized message (hex): ";
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(serialized[i]);
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << ' ';
  }
  std::cout << std::dec << std::setfill(' ') << "" << std::endl; // reset formatting
}

const char* Packet::serialize() const {
  serialized_buffer.clear();
  serialized_buffer.reserve(serializedSize());
  
  // Write the message type (1 byte)
  serialized_buffer.push_back(static_cast<char>(m_type));
  
  // Write the number of messages (1 byte)
  serialized_buffer.push_back(static_cast<char>(nb_mes));
  
  // Serialize payload with network byte order conversion
  // std::visit([this](const auto& data) {
  //   using T = std::decay_t<decltype(data)>;
    
  //   if constexpr (std::is_same_v<T, std::vector<std::pair<pkt_seq_t, Message>>>) {
  //     // MES type
  for (const auto& [pkt, msg] : payload) {
    // Sequence number
    msg_seq_t pkt_network = convertToNetwork(pkt);
    const char* p = reinterpret_cast<const char*>(&pkt_network);
    serialized_buffer.insert(serialized_buffer.end(), p, p + sizeof(pkt_network));
    
    // serialize message
    msg.serializeTo(serialized_buffer);
  }
  //   } else {
  //     // ACK type
  //     for (const auto& seq : data) {
  //       msg_seq_t seq_network = convertToNetwork(seq);
  //       serialized_buffer.insert(serialized_buffer.end(),
  //                               reinterpret_cast<const char*>(&seq_network),
  //                               reinterpret_cast<const char*>(&seq_network) + sizeof(seq_network));
  //     }
  //   }
  // }, payload);
  
  return serialized_buffer.data();
}

Packet Packet::deserialize(const char* buffer) {
  size_t offset = 0;
  
  // STEP 1: Read type (1 byte)
  MessageType type = static_cast<MessageType>(buffer[offset++]);
  
  // STEP 2: Read nb_mes (1 byte)
  uint8_t nb = static_cast<uint8_t>(buffer[offset++]);
  
  // if (type == MessageType::MES) {
  std::vector<std::pair<pkt_seq_t, Message>> payloads;
  for (uint8_t i = 0; i < nb; ++i) {
    pkt_seq_t pkt;
    msg_seq_t seq;
    proc_id_t orig;
    
    // Read and convert from network byte order
    pkt_seq_t pkt_network;
    std::memcpy(&pkt_network, buffer + offset, sizeof(pkt_network));
    pkt = convertFromNetwork(pkt_network);
    offset += sizeof(pkt);

    Message msg = Message::deserialize(buffer, offset);

    payloads.emplace_back(pkt, msg);
  }

  return Packet(type, nb, std::move(payloads));
  // } else { // ACK
  //   std::vector<msg_seq_t> seqs;
  //   for (uint8_t i = 0; i < nb; ++i) {
  //     msg_seq_t seq_network;
  //     std::memcpy(&seq_network, buffer + offset, sizeof(seq_network));
  //     msg_seq_t seq = convertFromNetwork(seq_network);
  //     offset += sizeof(seq);
  //     seqs.push_back(seq);
  //   }
  //   return Packet(type, nb, std::move(seqs));
  // }
}
