#include "message.hpp"

// Constructor for MES
Packet::Packet(MessageType type, uint8_t nb_m, 
                 std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> payloads)
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

const std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>>& Packet::getPayloads() const 
{
  return payload;
  // assert(m_type == MessageType::MES);
  // return std::get<0>(payload);
}

const std::vector<msg_seq_t> Packet::getSeqs() const
{ 
  std::vector<msg_seq_t> seqs;
  seqs.reserve(nb_mes);
  for (const auto& [seq, _, __] : payload) {
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
  size_t size = 1 + 1;

  // if (m_type == MES) {
  size += nb_mes * (sizeof(msg_seq_t) + sizeof(proc_id_t) + sizeof(msg_seq_t));
  // } else if (m_type == ACK) {
  //   size += nb_mes * sizeof(msg_seq_t);
  // }

  return size;
}

void Packet::displayPacket(const Packet& message)
{
  // std::cout << "Packet Type: " << static_cast<int>(message.getType()) << "" << std::endl;
  // std::cout << "Number of Packets: " << static_cast<int>(message.getNbMes()) << "" << std::endl;
  // std::cout << "Payload:" << std::endl;
  // if (message.getType() == MES) {
  for (const auto& [seq, id, payload] : message.getPayloads()) {
    // std::cout << "    ";
    // std::cout << seq<< " ";
    // std::cout << id << " ";
    // std::cout << payload << "" << std::endl;
    }
  // } else if (message.getType() == ACK) {
  //   for (msg_seq_t seq : message.getSeqs()) {
  //     // std::cout << seq << " ";
  //   }
  // }
  // std::cout << "" << std::endl;
}

void Packet::displaySerialized(const char* serialized)
{
  size_t len = Packet::deserialize(serialized).serializedSize();
  // std::cout << "Serialized message size: " << len << std::endl;
  // std::cout << "Serialized message (hex): ";
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(serialized[i]);
    // std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << ' ';
  }
  // std::cout << std::dec << std::setfill(' ') << "" << std::endl; // reset formatting
}

const char* Packet::serialize() const {
  serialized_buffer.clear();
  
  // Write the message type (1 byte)
  serialized_buffer.push_back(static_cast<char>(m_type));
  
  // Write the number of messages (1 byte)
  serialized_buffer.push_back(static_cast<char>(nb_mes));
  
  // Serialize payload with network byte order conversion
  // std::visit([this](const auto& data) {
  //   using T = std::decay_t<decltype(data)>;
    
  //   if constexpr (std::is_same_v<T, std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>>>) {
  //     // MES type
  for (const auto& [seq, orig, payload_data] : payload) {
    // Convert each field to network byte order before serializing
    
    // Sequence number
    msg_seq_t seq_network = convertToNetwork(seq);
    serialized_buffer.insert(serialized_buffer.end(),
                            reinterpret_cast<const char*>(&seq_network),
                            reinterpret_cast<const char*>(&seq_network) + sizeof(seq_network));
    
    // Origin ID
    proc_id_t orig_network = convertToNetwork(orig);
    serialized_buffer.insert(serialized_buffer.end(),
                            reinterpret_cast<const char*>(&orig_network),
                            reinterpret_cast<const char*>(&orig_network) + sizeof(orig_network));
    
    // Payload
    msg_seq_t payload_network = convertToNetwork(payload_data);
    serialized_buffer.insert(serialized_buffer.end(),
                            reinterpret_cast<const char*>(&payload_network),
                            reinterpret_cast<const char*>(&payload_network) + sizeof(payload_network));
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
  std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> payloads;
  for (uint8_t i = 0; i < nb; ++i) {
    msg_seq_t seq, payload_data;
    proc_id_t orig;
    
    // Read and convert from network byte order
    msg_seq_t seq_network;
    std::memcpy(&seq_network, buffer + offset, sizeof(seq_network));
    seq = convertFromNetwork(seq_network);
    offset += sizeof(seq);
    
    proc_id_t orig_network;
    std::memcpy(&orig_network, buffer + offset, sizeof(orig_network));
    orig = convertFromNetwork(orig_network);
    offset += sizeof(orig);
    
    msg_seq_t payload_network;
    std::memcpy(&payload_network, buffer + offset, sizeof(payload_network));
    payload_data = convertFromNetwork(payload_network);
    offset += sizeof(payload_data);
    
    payloads.emplace_back(seq, orig, payload_data);
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
