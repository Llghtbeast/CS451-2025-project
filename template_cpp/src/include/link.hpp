#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <set>

#include "parser.hpp"

enum MessageType : uint8_t {
  MES = 0,
  ACK = 1,
  NACK = 2
};

/**
 * Base class representing the endpoints of a communication link with send and receive capabilities.
 */
class Link {
public:
  Link(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  static std::vector<char> encodeMessage(uint32_t m_seq, uint8_t message_type);
  static std::pair<uint32_t, uint8_t> decodeMessage(const std::vector<char>& buffer);
  
protected:
  int socket;
  sockaddr_in source_addr;
  sockaddr_in dest_addr;
public:
  static constexpr uint32_t window_size = 1; // TODO: increase window size for performance, need to implement more complex logic
  static constexpr int32_t buffer_size = 1 + sizeof(uint32_t); // 1 byte for message type + 4 bytes for m_seq
};

/**
 * Derived class representing a sender endpoint of a communication link with message queuing and sending capabilities.
 */
class SenderLink : public Link {
public:
  SenderLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  bool enqueueMessage();
  void send();
  void receiveAck(uint32_t m_seq);

private:
  uint32_t m_seq = 0;
  std::set<uint32_t> messageQueue;
  size_t maxQueueSize = window_size * 100;
};

/**
 * Derived class representing a receiver endpoint of a communication link with message receiving and delivering capabilities.
 */
class ReceiverLink : public Link {
public:
  ReceiverLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  bool deliver(uint32_t m_seq);

private:
  uint32_t last_delivered = 0;
};