#pragma once

#include <vector>
#include <stdint.h>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>

enum MessageType : uint8_t {
  MES = 0,
  ACK = 1,
  NACK = 2
};

class Message {
public:
  Message(MessageType type, uint8_t nb_m, std::vector<uint32_t> seqs);

  MessageType getType() const;
  uint8_t getNbMes() const;
  const std::vector<uint32_t> getSeqs() const;

  Message toAck();
  size_t serializedSize() const;

  const char * serialize() const;
  static Message deserialize(const char * buffer);

  static constexpr uint32_t max_msgs = 8; 
  static constexpr int32_t max_size = 1 + 1 + max_msgs * sizeof(uint32_t);

private:
  MessageType m_type;
  uint8_t nb_mes; // maximum 8 messages per Message
  std::vector<uint32_t> seqs;

  // buffer that holds the serialized bytes so serialize() can return a stable pointer
  mutable std::vector<char> serialized_buffer;
};