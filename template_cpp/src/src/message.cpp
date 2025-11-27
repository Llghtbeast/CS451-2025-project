#include "message.hpp"
#include <iostream>

Message::Message(MessageType type, unsigned long origin_id, uint8_t nb_m, std::vector<uint32_t> seqs)
  : m_type(type), origin_id(origin_id), nb_mes(nb_m), seqs(seqs)
{
  // basic validation
  if (nb_mes != seqs.size()) {
    throw std::invalid_argument("Message: nb_mes does not match seqs sizes");
  }
}

MessageType Message::getType() const                  { return m_type; }
unsigned long Message::getOriginId() const            { return origin_id; }
uint8_t Message::getNbMes() const                     { return nb_mes; }
const std::vector<uint32_t> Message::getSeqs() const  { return seqs; }

/** 
 * Convert message to ACK type
 */
Message Message::toAck()
{
  Message new_msg = Message(ACK, origin_id, nb_mes, seqs);
  return new_msg;
}

size_t Message::serializedSize() const 
{
  return 1 + sizeof(origin_id) + 1 + nb_mes * sizeof(uint32_t); // type + origin_id(8) + nb_mes + seqs
}

/**
 * Serialize the Message into a byte buffer.
 * @return Pointer to the serialized byte buffer.
 */ 
const char *Message::serialize() const
{
  serialized_buffer.clear();
  serialized_buffer.reserve(serializedSize());

  // header: type (1 byte)
  serialized_buffer.push_back(static_cast<char>(m_type));

  // origin_id in network (big-endian) order (8 bytes)
  uint64_t net_id = htobe64(static_cast<uint64_t>(origin_id));
  char id_bytes[sizeof(net_id)];
  std::memcpy(id_bytes, &net_id, sizeof(net_id));
  serialized_buffer.insert(serialized_buffer.end(), id_bytes, id_bytes + sizeof(net_id));

  // nb_mes (1 byte)
  serialized_buffer.push_back(static_cast<char>(nb_mes));

  // seqs (each 4 bytes big-endian)
  for (size_t i = 0; i < nb_mes; ++i) {
    uint32_t net_seq = htonl(seqs[i]);
    char seq_bytes[sizeof(net_seq)];
    std::memcpy(seq_bytes, &net_seq, sizeof(net_seq));
    serialized_buffer.insert(serialized_buffer.end(), seq_bytes, seq_bytes + sizeof(net_seq));
  }

  return serialized_buffer.data();
}

Message Message::deserialize(const char * buffer)
{
  if (buffer == nullptr) throw std::invalid_argument("Message::deserialize: null buffer");

  const unsigned char *ptr = reinterpret_cast<const unsigned char *>(buffer);
  size_t offset = 0;

  // type (1 byte)
  MessageType type = static_cast<MessageType>(ptr[offset]);
  offset += 1;

  // origin_id (8 bytes, big-endian)
  uint64_t net_id;
  std::memcpy(&net_id, ptr + offset, sizeof(net_id));
  offset += sizeof(net_id);
  unsigned long origin_id = static_cast<unsigned long>(be64toh(net_id));

  // nb_mes (1 byte)
  uint8_t nb = static_cast<uint8_t>(ptr[offset]);
  offset += 1;

  std::vector<uint32_t> seqs_out;
  seqs_out.reserve(nb);

  for (uint8_t i = 0; i < nb; ++i) {
    uint32_t net_seq;
    std::memcpy(&net_seq, ptr + offset, sizeof(net_seq));
    offset += sizeof(net_seq);
    uint32_t seq = ntohl(net_seq);
    seqs_out.push_back(seq);
  }

  return Message(type, origin_id, nb, seqs_out);
}
