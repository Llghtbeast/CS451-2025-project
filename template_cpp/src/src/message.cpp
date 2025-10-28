#include "message.hpp"

Message::Message(MessageType type, uint8_t nb_m, std::vector<uint32_t> seqs)
  : m_type(type), nb_mes(nb_m), seqs(seqs)
{
  // basic validation
  if (nb_mes != seqs.size()) {
    throw std::invalid_argument("Message: nb_mes does not match seqs sizes");
  }
}

MessageType Message::getType() const { return m_type; }
uint8_t Message::getNbMes() const { return nb_mes; }
const std::vector<uint32_t> Message::getSeqs() const { return seqs; }

/** 
 * Convert message to ACK type
 */
Message Message::toAck()
{
  Message new_msg = Message(ACK, nb_mes, seqs);
  return new_msg;
}

size_t Message::serializedSize() const 
{
  return 1 + 1 + nb_mes * sizeof(uint32_t); // type + nb_mes + seqs
}

/**
 * Serialize the Message into a byte buffer.
 * @return Pointer to the serialized byte buffer.
 */ 
const char *Message::serialize() const
{
  serialized_buffer.clear();

  // header: type (1 byte) + nb_mes (1 byte)
  serialized_buffer.push_back(static_cast<char>(m_type));
  serialized_buffer.push_back(static_cast<char>(nb_mes));

  for (size_t i = 0; i < nb_mes; ++i) {
    // sequence number in network byte order
    uint32_t net_seq = htonl(seqs[i]);
    char seq_bytes[4];
    std::memcpy(seq_bytes, &net_seq, sizeof(net_seq));
    serialized_buffer.insert(serialized_buffer.end(), seq_bytes, seq_bytes + sizeof(net_seq));
  }

  // Return pointer to the internal buffer (valid until next non-const serialize() call)
  return serialized_buffer.data();
}

Message Message::deserialize(const char * buffer)
{
  if (buffer == nullptr) throw std::invalid_argument("Message::deserialize: null buffer");

  const unsigned char *ptr = reinterpret_cast<const unsigned char *>(buffer);
  size_t offset = 0;

  // type + nb_mes
  MessageType type = static_cast<MessageType>(ptr[offset]);
  offset += 1;
  uint8_t nb = static_cast<uint8_t>(ptr[offset]);
  offset += 1;

  std::vector<uint32_t> seqs_out;

  for (uint8_t i = 0; i < nb; ++i) {
    // read seq
    uint32_t net_seq;
    std::memcpy(&net_seq, ptr + offset, sizeof(net_seq));
    offset += sizeof(net_seq);
    uint32_t seq = ntohl(net_seq);
    seqs_out.push_back(seq);
  }

  return Message(type, nb, seqs_out);
}
