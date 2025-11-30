#pragma once

#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>

#include "parser.hpp"
#include "link.hpp"
#include "helper.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "sets.hpp"
#include "maps.hpp"
#include "globals.hpp"

/**
 * Implementation of a network node that can send and receive messages.
 */
class Node {
public:  
  /**
   * Constructor to initialize the network node with its neighbors, its ID, the network's receiver ID, and the file output path. It sets up the UDP socket and binds it to the node's address.
   * @param nodes A vector of all nodes in the network.
   * @param id The unique identifier for this node.
   * @param receiver_id The unique identifier for the network's receiver node.
   * @param outputPath The path to the output file where messages will be logged.
   */
  Node(std::vector<Parser::Host> nodes, proc_id_t id, std::string outputPath);
  
  /**
   * Starts the node's sending and listening threads.
   */
  void start();

  // temporary method for testing
  void enqueueMessage(sockaddr_in dest);

  /**
   * Enqueues a message to be broadcast
   * @param origin_id The ID of the origin node of the message. If it is this node's ID, a new sequence number is generated. Otherwise, the provided sequence number is used.
   * @param seq The sequence number of the message. If origin_id is this node's ID, this parameter is ignored.
   */
  void broadcast(proc_id_t origin_id, msg_seq_t seq = 0);

  /**
   * Determines if a message can be delivered based on the acknowledgments received from other nodes.
   * @param origin_id The ID of the origin node of the message.
   * @param seq The sequence number of the message.
   * @return true if the message can be delivered, false otherwise.
   */
  bool can_deliver(proc_id_t origin_id, msg_seq_t seq);

  /**
   * Terminates the node's sending and listening threads.
   */
  void terminate();

  /**
   * Cleans up resources used by the node, including closing the socket and output file.
   */
  void cleanup();
  
  /**
   * Flushes the output stream to ensure all received messages are logged.
   */
  void flushToOutput();

  void allMessagesEnqueued(sockaddr_in dest);
  void finished(sockaddr_in dest);

private:
  /**
   * Message sending loop that continuously enqueues and sends messages to the specified destination address while the run flag is set.
   */
  void send();

  /**
   * Message listening loop that continuously listens for incoming messages and processes them while the run flag is set.
   */
  void listen();

  /**
   * Logger thread function that periodically writes log entries to the log file while the run flag is set.
   */
  void log();
  
private:
  msg_seq_t m_seq = 0;

  proc_id_t id;
  std::unique_ptr<Logger> logger;
  std::atomic_bool runFlag;

  int node_socket;
  sockaddr_in node_addr;
  
  size_t nb_nodes;
  std::unordered_map<std::string, proc_id_t> others_id;
  std::unordered_map<std::string, std::unique_ptr<PerfectLink>> links;

  std::unordered_map<proc_id_t, ConcurrentSet<msg_seq_t>> pending_messages;
  std::unordered_map<proc_id_t, msg_seq_t> next_expected_msg;
  std::unordered_map<proc_id_t, ConcurrentMap<msg_seq_t, std::set<proc_id_t>>> acked_by;

  std::thread sender_thread;
  std::thread listener_thread;
  std::thread logger_thread;
};