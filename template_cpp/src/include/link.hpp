#pragma once

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
#include <memory>
#include <tuple>

#include "parser.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "sets.hpp"

/**
 * Derived class representing a sender endpoint of a communication link.
 */
class Port {
public:
/**
 * Constructor to initialize the Port with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
  Port(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);

protected:
  int socket;
  sockaddr_in source_addr;
  sockaddr_in dest_addr;

public:
  static constexpr msg_seq_t window_size = SEND_WINDOW_SIZE; 
};

/**
 * Derived class representing a sender endpoint of a communication link with message queuing and sending capabilities.
 */
class SenderPort: public Port {
public:
/**
 * Constructor to initialize the SenderPort with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
  SenderPort(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  
  /**
   * Enqueues a message to be sent later.
   * @param origin_id The ID of the origin node.
   * @param m_seq The sequence number of the message.
   */
  void enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq);

/**
 * Send the first enqueued messages.
 * @throws std::runtime_error if sending fails.
 */
  void send();
  
/** 
 * Receive ACK from receiver and removed corresponding message from queue.
 * @param m_seq The sequence number of the acknowledged message.
*/
  void receiveAck(std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> acked_messages);

  void allMessagesEnqueued();

  void finished();
  
private:
  msg_seq_t link_seq = 0;

  ConcurrentSet<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>, TupleFirstElementComparator> messageQueue;
    
  std::mutex finished_mutex;
  std::condition_variable finished_cv;
  bool all_msg_enqueued = false;
  bool all_msg_delivered = false;
};

/**
 * Derived class representing a receiver endpoint of a communication link with message receiving and delivering capabilities.
 */
class ReceiverPort: public Port {
public:
/**
 * Constructor to initialize the ReceiverPort with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
  ReceiverPort(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  
/**
 * Add message to delivered list, send ACK to sender, and return true if message was not already delivered. Otherwise, return false.
 * @param message The message to respond to.
 */
  std::vector<bool> respond(Message message);
  
  private:
  SlidingSet<msg_seq_t> deliveredMessages;
};

/**
 * Base class representing the endpoints of a perfect link implementation with send and receive capabilities.
 */
class PerfectLink {
public:
/**
 * Constructor to initialize the PerfectLink with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
  PerfectLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);

  void enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq);
  void send();
  std::vector<bool> receive(Message mes);

  // Functions for measuring performance of solution
  void allMessagesEnqueued();
  void finished();

private:
  SenderPort sendPort;
  ReceiverPort recvPort;
};