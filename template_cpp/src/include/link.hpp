#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <condition_variable>
#include <mutex>
#include <errno.h>

#include "parser.hpp"
#include "message.hpp"

/**
 * Base class representing the endpoints of a communication link with send and receive capabilities.
 */
class Link {
public:
  Link(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);

protected:
  int socket;
  sockaddr_in source_addr;
  sockaddr_in dest_addr;
public:
  static constexpr uint32_t window_size = 8; // TODO: increase window size for performance, need to implement more complex logic
};

/**
 * Derived class representing a sender endpoint of a communication link with message queuing and sending capabilities.
 */
class SenderLink : public Link {
public:
  SenderLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  uint32_t enqueueMessage();
  void send();
  void receiveAck(std::vector<uint32_t> acked_messages);
  void allMessagesEnqueued();
  void finished();

private:
  uint32_t m_seq = 0;
  std::set<uint32_t> messageQueue;
  size_t maxQueueSize = Message::max_msgs * window_size * 100;

  std::condition_variable queue_cv;
  std::mutex queue_mutex;

  std::mutex finished_mutex;
  std::condition_variable finished_cv;
  bool all_msg_enqueued = false;
  bool all_msg_delivered = false;
};

/**
 * Derived class representing a receiver endpoint of a communication link with message receiving and delivering capabilities.
 */
class ReceiverLink : public Link {
public:
  ReceiverLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  std::vector<bool> respond(Message messages);

private:
  std::set<uint32_t> deliveredMessages;
};