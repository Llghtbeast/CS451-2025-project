#pragma once

#include <vector>
#include <stdint.h>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <endian.h>
#include <iostream>
#include <iomanip>
#include <variant>
#include <cassert>

#include "globals.hpp"
#include "helper.hpp"

enum MessageType : uint8_t {
  MES = 0,
  ACK = 1,
  NACK = 2
};

/**
 * Class representing a network message with serialization and deserialization capabilities.
 * TODO: reduce size of ACK messages by only including link sequence numbers.
 * Messages of type MES contain a list of tuples of link sequence number, origin id, and message sequence number.
 * Messages of type ACK contain a list of message sequence numbers being acknowledged. 
 */
class Message {
public:
  // Constructor for MES type
  Message(MessageType type, uint8_t nb_m, 
          std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> payloads);
  
  // Constructor for ACK type
  // Message(MessageType type, uint8_t nb_m, 
  //         std::vector<msg_seq_t> seqs);

  MessageType getType() const;
  uint8_t getNbMes() const;
  // proc_id_t getOriginId() const;

  // For MES messages
  const std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>>& getPayloads() const;
  
  // For ACK messages
  const std::vector<msg_seq_t> getSeqs() const;

  Message toAck();
  size_t serializedSize() const;

  // Debugging functions for displaying messages
  static void displayMessage(const Message& message);
  static void displaySerialized(const char* serialized);

  const char * serialize() const;
  static Message deserialize(const char * buffer);

  static constexpr msg_seq_t max_msgs = MAX_MESSAGES_PER_PACKET;
  static constexpr int32_t mes_max_size = 1 + 1 +  max_msgs * (sizeof(msg_seq_t) + sizeof(proc_id_t) + sizeof(msg_seq_t));
  static constexpr int32_t ack_max_size = 1 + 1 +  max_msgs * (sizeof(msg_seq_t));
  static constexpr size_t max_size = ack_max_size > mes_max_size ? ack_max_size : mes_max_size;

private:
  MessageType m_type;
  proc_id_t origin_id;
  uint8_t nb_mes; // maximum 8 messages per Message
  std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> payload;

  // Payload for MES messages or sequence numbers for ACK messages
  // std::variant<
  //   std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>>, // for MES type
  //   std::vector<msg_seq_t>                                    // for ACK type
  // > payload;

  // buffer that holds the serialized bytes so serialize() can return a stable pointer
  mutable std::vector<char> serialized_buffer;

  // Helper functions to choose the right conversion based on size
  template<typename T>
  static T convertToNetwork(T value) {
    if constexpr (sizeof(T) == 1) {
      return value; // Single byte, no conversion needed
    } else if constexpr (sizeof(T) == 2) {
      return htons(value);
    } else if constexpr (sizeof(T) == 4) {
      return htonl(value);
    } else if constexpr (sizeof(T) == 8) {
      return htonll(value);
    } else {
      static_assert(sizeof(T) <= 8, "Unsupported type size");
    }
  }

  template<typename T>
  static T convertFromNetwork(T value) {
    if constexpr (sizeof(T) == 1) {
      return value;
    } else if constexpr (sizeof(T) == 2) {
      return ntohs(value);
    } else if constexpr (sizeof(T) == 4) {
      return ntohl(value);
    } else if constexpr (sizeof(T) == 8) {
      return ntohll(value);
    } else {
      static_assert(sizeof(T) <= 8, "Unsupported type size");
    }
  }
};