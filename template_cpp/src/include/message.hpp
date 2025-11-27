#pragma once

#include <vector>
#include <stdint.h>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <endian.h>
#include <iostream>
#include <iomanip>

#include "globals.hpp"

enum MessageType : uint8_t {
  MES = 0,
  ACK = 1,
  NACK = 2
};

class Message {
public:
  Message(MessageType type, proc_id_t origin_id, uint8_t nb_m, std::vector<msg_seq_t> seqs);

  MessageType getType() const;
  proc_id_t getOriginId() const;
  uint8_t getNbMes() const;
  const std::vector<msg_seq_t> getSeqs() const;

  Message toAck();
  size_t serializedSize() const;

  // Debugging functions for displaying messages
  static void displayMessage(const Message& message);
  static void displaySerialized(const char* serialized);

  const char * serialize() const;
  static Message deserialize(const char * buffer);

  static constexpr msg_seq_t max_msgs = MAX_MESSAGES_PER_PACKET; 
  static constexpr int32_t max_size = 1 + sizeof(proc_id_t) + 1 + max_msgs * sizeof(msg_seq_t);

private:
  MessageType m_type;
  proc_id_t origin_id;
  uint8_t nb_mes; // maximum 8 messages per Message
  std::vector<msg_seq_t> seqs;

  // buffer that holds the serialized bytes so serialize() can return a stable pointer
  mutable std::vector<char> serialized_buffer;
};