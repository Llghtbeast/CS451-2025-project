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
#include <utility>

#include "parser.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "sets.hpp"
#include "deque.hpp"


/**
 * Base class representing the endpoints of a perfect link implementation with send and receive capabilities.
 */
class PerfectLink {
  public:
  /**
   * Constructor to initialize the PerfectLink with a socket and its send and receive addresses.
   * @param socket The UDP socket used for communication.
   * @param source_addr The address to which packets will be sent.
   * @param dest_addr The address from which packets will be received.
   */
  PerfectLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr);
  
  /**
   * Enqueues a packet to be sent later.
   * @param origin_id The ID of the origin node.
   * @param m_seq The sequence number of the packet.
   */
  void enqueueMessage(proc_id_t origin_id, msg_seq_t m_seq);
  /**
  * Send the first enqueued packets.
  * @throws std::runtime_error if sending fails.
  */
  void send();

  /** 
    * Receive ACK from receiver and removed corresponding packet from queue.
    * @param m_seq The sequence number of the acknowledged packet.
    * Add packet to delivered list, send ACK to sender, and return true if packet was not already delivered. Otherwise, return false.
    * @param packet The packet to respond to.
    */
  std::vector<bool> receive(Packet packet);

private:
  int socket;
  sockaddr_in source_addr;
  sockaddr_in dest_addr;

  // Sending
  msg_seq_t link_seq = 0;
  
  ConcurrentDeque<std::pair<pkt_seq_t, Message>> message_queue;
  ConcurrentSet<std::pair<pkt_seq_t, Message>, TupleFirstElementComparator> pending_msgs;
  
  // Reception
  SlidingSet<msg_seq_t> deliveredPackets;
  
public:
  static constexpr msg_seq_t window_size = SEND_WINDOW_SIZE; 
};