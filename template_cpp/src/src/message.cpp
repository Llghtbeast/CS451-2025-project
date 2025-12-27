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

void Message::serializeTo(char *buffer, size_t &offset) const
{
  msg_seq_t seq_network = convertToNetwork(seq);
  proc_id_t orig_network = convertToNetwork(origin);
  
  std::memcpy(buffer + offset, &seq_network, sizeof(seq_network));
  offset += sizeof(seq_network);
  
  std::memcpy(buffer + offset, &orig_network, sizeof(orig_network));
  offset += sizeof(orig_network);
}

Message Message::deserialize(const char *buffer, size_t &offset)
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
                const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& seqs,
                const std::array<Message, MAX_MESSAGES_PER_PACKET>& msgs)
    : m_type(type), nb_mes(nb_m), payload(MesPayload{seqs, msgs})
{
  assert(type == MessageType::MES); 
  assert(nb_m <= MAX_MESSAGES_PER_PACKET);
}

Packet::Packet(MessageType type, uint8_t nb_m, 
                const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& seqs)
    : m_type(type), nb_mes(nb_m), payload(seqs) 
{
  assert(type == MessageType::ACK);
  assert(nb_m <= MAX_MESSAGES_PER_PACKET);
}

MessageType Packet::getType() const  { return m_type; }
uint8_t Packet::getNbMes() const     { return nb_mes; }

const std::array<Message, MAX_MESSAGES_PER_PACKET>& Packet::getMessages() const 
{
  assert(m_type == MessageType::MES);
  return std::get<0>(payload).msgs;
}

const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& Packet::getSeqs() const
{ 
  if (m_type == MessageType::MES) 
    return std::get<0>(payload).seqs; 
  else
    return std::get<1>(payload);
}

/** 
 * Convert message to ACK type
 */
Packet Packet::toAck()
{
  assert (m_type == MES);

  const auto& data = std::get<0>(payload);
  return Packet(ACK, nb_mes, data.seqs);
}

size_t Packet::serializedSize() const 
{
  size_t size = sizeof(m_type) + sizeof(nb_mes);

  if (m_type == MES) {
    size += nb_mes * (sizeof(pkt_seq_t) + Message::serializedSize);
  } else if (m_type == ACK) {
    size += nb_mes * sizeof(pkt_seq_t);
  }
  return size;
}

void Packet::displayPacket()
{
  std::cout << "Packet Type: " << static_cast<int>(m_type) << "" << std::endl;
  std::cout << "Number of Packets: " << static_cast<int>(nb_mes) << "" << std::endl;
  std::cout << "Payload:\n";
  if (m_type == MessageType::MES)
  {
    const auto& data = std::get<0>(payload);
    for (size_t i = 0; i < nb_mes; i++)
    {
      std::cout << "    pkt_seq: " << data.seqs[i] << " ";
      Message::displayMessage(data.msgs[i]);
    }
  }
  else
  {
    std::cout << "    pkt_seqs: ";
    const auto& data = std::get<1>(payload);
    for (const auto& pkt : data) {
      std::cout << pkt << " ";
    }
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
  size_t offset = 0;
  // Write the message type (1 byte)
  serialized_buffer[offset++] = static_cast<char>(m_type);
  
  // Write the number of messages (1 byte)
  serialized_buffer[offset++] = static_cast<char>(nb_mes);
  
  if (m_type == MES) 
  {
    const auto& data = std::get<0>(payload);
    for (size_t i = 0; i < nb_mes; i++)
    {
      // Sequence number
      msg_seq_t pkt_network = convertToNetwork(data.seqs[i]);
      std::memcpy(&serialized_buffer[offset], &pkt_network, sizeof(pkt_network));
      offset += sizeof(pkt_network);
      
      // serialize message
      data.msgs[i].serializeTo(serialized_buffer.data(), offset);
    }
  }
  else
  {
    const auto& data = std::get<1>(payload);
    for (const auto& pkt : data) {
      // Sequence number
      msg_seq_t pkt_network = convertToNetwork(pkt);
      std::memcpy(&serialized_buffer[offset], &pkt_network, sizeof(pkt_network));
      offset += sizeof(pkt_network);
    }
  }

  return serialized_buffer.data();
}

Packet Packet::deserialize(const char* buffer) {
  size_t offset = 0;
  
  // STEP 1: Read type (1 byte)
  MessageType type = static_cast<MessageType>(buffer[offset++]);
  
  // STEP 2: Read nb_mes (1 byte)
  uint8_t nb = static_cast<uint8_t>(buffer[offset++]);
  
  if (type == MessageType::MES) 
  {
    std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;
    std::array<Message, MAX_MESSAGES_PER_PACKET> msgs;

    for (uint8_t i = 0; i < nb; ++i) {
      pkt_seq_t pkt;
      
      // Read and convert from network byte order
      pkt_seq_t pkt_network;
      std::memcpy(&pkt_network, buffer + offset, sizeof(pkt_network));
      offset += sizeof(pkt_network);

      seqs[i] = convertFromNetwork(pkt_network);
      msgs[i] = Message::deserialize(buffer, offset);
    }

    return Packet(MES, nb, seqs, msgs);
  } 
  else
  {
    std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;

    for (uint8_t i = 0; i < nb; ++i) {
      msg_seq_t seq_network;
      std::memcpy(&seq_network, buffer + offset, sizeof(seq_network));
      offset += sizeof(seq_network);
      seqs[i] = convertFromNetwork(seq_network);
    }
    return Packet(ACK, nb, seqs);
  }
}
