#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <set>

#include "parser.hpp"

/**
 * Base class representing the endpoints of a communication link with send and receive capabilities.
 */
class Link {
public:
  Link(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  std::vector<char> encodeMessage(uint32_t m_seq, uint8_t message_type);
  
protected:
  int socket;
  sockaddr_in source_addr;
  sockaddr_in dest_addr;
  const uint32_t window_size = 1;
public:
  static const uint32_t buffer_size = 1 + sizeof(uint32_t); // 1 byte for message type + 4 bytes for m_seq
};

/**
 * Derived class representing a sender endpoint of a communication link with message queuing and sending capabilities.
 */
class SenderLink : public Link {
public:
  void enqueueMessage(uint32_t m_seq); //, std::string msg = "");
  void send();
  void receiveAck(uint32_t m_seq);

private:
  // std::unordered_map<uint32_t, std::string> messages;
  std::set<uint32_t> messageQueue;
};

/**
 * Derived class representing a receiver endpoint of a communication link with message receiving and delivering capabilities.
 */
class ReceiverLink : public Link {
public:
  bool deliver(uint32_t m_seq);

private:
  uint32_t last_delivered = 0;
};