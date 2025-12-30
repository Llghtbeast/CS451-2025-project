#pragma once

#include <vector>
#include <array>
#include <stdint.h>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <endian.h>
#include <iostream>
#include <iomanip>
#include <variant>
#include <cassert>
#include <set>
#include <memory>

#include "globals.hpp"
#include "helper.hpp"

enum MessageType : uint8_t {
  MES = 0,
  ACK = 1,
  NACK = 2,
};

// ======================== Node message class ========================
class Message {
public:
  // Constructor
  Message() = default;
  Message(MessageType type, prop_nb_t instance, prop_nb_t round, const std::set<proposal_t>& proposal_set);
  bool operator==(const Message& other) const;

  // Response generation
  Message toAck() const;
  Message toNack(std::set<proposal_t>& completed_proposal_set) const;

  // helper to display
  void displayMessage() const;

  // Serialization methods
  size_t serializedSize() const;
  void serializeTo(char* buffer, size_t& offset) const;
  static Message deserialize(const char * buffer, size_t& offset);

public:
  MessageType type;
  prop_nb_t instance;
  prop_nb_t round;
  std::vector<proposal_t> proposed_values;

  static constexpr size_t max_serialized_size = sizeof(instance) + sizeof(type) + sizeof(round) + sizeof(proposal_t) * MAX_PROPOSAL_SET_SIZE;
};

// ======================== Link packet class ======================== 
struct MesPayload {
  std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs;
  std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET> msgs;
};

/**
 * Class representing a network packet with serialization and deserialization capabilities.
 * Packets of type MES contain a list of tuples of link sequence number, pointer to Message objects.
 * Packets of type ACK contain a list of link sequence numbers being acknowledged. 
 */
class Packet {
public:
  // Constructor for MES type
  Packet(MessageType type, uint8_t nb_m,
          const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& seqs,
          const std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET>& msgs);
  // Constructor for ACK type
  Packet(MessageType type, uint8_t nb_m, 
          const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& seqs);

  
  MessageType getType() const;
  uint8_t getNbMes() const;

  // For MES packets
  const std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET>& getMessages() const;  
  // For all packets
  const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>& getSeqs() const;

  Packet toAck();
  size_t serializedSize() const;

  // Debugging functions for displaying packets
  void displayPacket();
  static void displaySerialized(const char* serialized);

  const char * serialize() const;
  static Packet deserialize(const char * buffer);

  static constexpr size_t max_msgs = MAX_MESSAGES_PER_PACKET;
  static constexpr size_t pkt_max_serialized_size = sizeof(MessageType) + sizeof(uint8_t) +  max_msgs * (sizeof(pkt_seq_t) + Message::max_serialized_size);
  static constexpr size_t ack_max_serialized_size = sizeof(MessageType) + sizeof(uint8_t) +  max_msgs * (sizeof(pkt_seq_t));
  static constexpr size_t max_serialized_size = ack_max_serialized_size > pkt_max_serialized_size ? ack_max_serialized_size : pkt_max_serialized_size;

private:
  MessageType m_type;
  uint8_t nb_mes; // maximum 8 packets per Packet

  // sequence numbers and messages for MES packets or sequence numbers for ACK packets
  std::variant<MesPayload, std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>> payload;

  mutable std::array<char, max_serialized_size> serialized_buffer;
};

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