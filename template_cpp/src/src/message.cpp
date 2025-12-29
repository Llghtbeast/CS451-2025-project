#include "message.hpp"

// =================== Message implementation =================== 
Message::Message(MessageType type, prop_nb_t round, const std::set<proposal_t>& proposal_set)
  : type(type), round(round), proposed_values(proposal_set.begin(), proposal_set.end())
{}

bool Message::operator==(const Message &other) const
{
  if (proposed_values.size() != other.proposed_values.size()) return false;
  for (size_t i = 0; i < proposed_values.size(); i++)
  {
    if (proposed_values[i] != other.proposed_values[i]) return false;
  }
  
  return (type == other.type) && (round == other.round);
}

Message Message::toAck() const
{
  return Message(MessageType::ACK, round, {});
}

Message Message::toNack(std::set<proposal_t>& completed_proposal_set) const
{
  return Message(MessageType::NACK, round, completed_proposal_set);
}

void Message::displayMessage() const
{
  std::cout << "Message: ";
  std::cout << "type=" << static_cast<int>(type) << ", ";
  std::cout << "round=" << static_cast<int>(round) << ", ";
  std::cout << "proposed_values = {";
  for (const auto& value: proposed_values)
  {
    std::cout << " " << value;
  }
  std::cout << " }\n";
}

size_t Message::serializedSize() const
{
  return sizeof(type) + sizeof(round) + sizeof(proposal_t) * proposed_values.size();
}

void Message::serializeTo(char *buffer, size_t &offset) const
{
  // serialize message type
  std::memcpy(buffer + offset, &type, sizeof(type));
  offset += sizeof(type);
  
  // serialize message round
  prop_nb_t round_network = convertToNetwork(round);
  std::memcpy(buffer + offset, &round_network, sizeof(round_network));
  offset += sizeof(round_network);
  
  // serialize message proposal set size and values
  uint16_t set_size_network = convertToNetwork(static_cast<uint16_t>(proposed_values.size()));
  std::memcpy(buffer + offset, &set_size_network, sizeof(set_size_network));
  offset += sizeof(set_size_network);

  for (const auto& value: proposed_values)
  {
    proposal_t prop_network = convertToNetwork(value);
    std::memcpy(buffer + offset, &prop_network, sizeof(prop_network));
    offset += sizeof(prop_network);
  }
}

Message Message::deserialize(const char *buffer, size_t &offset)
{
  Message msg;
  
  std::memcpy(&msg.type, buffer + offset, sizeof(msg.type));
  offset += sizeof(msg.type);
  
  prop_nb_t round_network;
  std::memcpy(&round_network, buffer + offset, sizeof(round_network));
  offset += sizeof(round_network);
  msg.round = convertFromNetwork(round_network);
  
  // Deserialize to proposed_values
  uint16_t set_size_network;
  std::memcpy(&set_size_network, buffer + offset, sizeof(set_size_network));
  offset += sizeof(set_size_network);
  uint16_t set_size = convertFromNetwork(set_size_network);
  msg.proposed_values.reserve(set_size);
  if (set_size > MAX_PROPOSAL_SET_SIZE) throw std::runtime_error("Deserilized set size exceeds maximum proposal set size");

  for (size_t i = 0; i < set_size; i++)
  {
    proposal_t prop_network;
    std::memcpy(&prop_network, buffer + offset, sizeof(prop_network));
    offset += sizeof(prop_network);
    msg.proposed_values.push_back(convertFromNetwork(prop_network));
  }

  return msg;
}

// =================== Packet implementation =================== 
// Constructor for MES
Packet::Packet(MessageType type, uint8_t nb_m,
                const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& seqs,
                const std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET>& msgs)
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

const std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET>& Packet::getMessages() const 
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
    auto& data = std::get<0>(payload);
    for (size_t i = 0; i < nb_mes; i++)
    {
      size += sizeof(pkt_seq_t) + data.msgs[i]->serializedSize();
    }
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
      data.msgs[i]->displayMessage();
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
      pkt_seq_t pkt_network = convertToNetwork(data.seqs[i]);
      std::memcpy(&serialized_buffer[offset], &pkt_network, sizeof(pkt_network));
      offset += sizeof(pkt_network);
      
      // serialize message
      data.msgs[i]->serializeTo(serialized_buffer.data(), offset);
    }
  }
  else
  {
    const auto& data = std::get<1>(payload);
    for (const auto& pkt : data) {
      // Sequence number
      pkt_seq_t pkt_network = convertToNetwork(pkt);
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
  if (nb > MAX_MESSAGES_PER_PACKET) throw std::runtime_error("Maximum message per packet bound exceeded in deserialization");
  
  if (type == MessageType::MES) 
  {
    std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;
    std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET> msgs;

    for (uint8_t i = 0; i < nb; ++i) {
      pkt_seq_t pkt;
      
      // Read and convert from network byte order
      pkt_seq_t pkt_network;
      std::memcpy(&pkt_network, buffer + offset, sizeof(pkt_network));
      offset += sizeof(pkt_network);

      seqs[i] = convertFromNetwork(pkt_network);
      msgs[i] = std::make_shared<Message>(Message::deserialize(buffer, offset));
    }

    return Packet(MES, nb, seqs, msgs);
  } 
  else
  {
    std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;

    for (uint8_t i = 0; i < nb; ++i) {
      pkt_seq_t seq_network;
      std::memcpy(&seq_network, buffer + offset, sizeof(seq_network));
      offset += sizeof(seq_network);
      seqs[i] = convertFromNetwork(seq_network);
    }
    return Packet(ACK, nb, seqs);
  }
}
