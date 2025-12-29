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

#include "globals.hpp"
#include "helper.hpp"
#include "parser.hpp"
#include "link.hpp"
#include "lattice_agreement.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "sets.hpp"
#include "maps.hpp"

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
  Node(std::vector<Parser::Host> nodes, proc_id_t id, std::string outputPath, uint32_t ds);
  
  // Destructor
  ~Node();

  /**
   * Starts the node's sending and listening threads.
   */
  void start();

  /**
   * Cleans up resources used by the node, including closing the socket and output file.
   */
  void cleanup();
  
  /**
   * Flushes the output stream to ensure all received messages are logged.
   */
  void flushToOutput();

  /**
   * Terminates the node's sending and listening threads.
   */
  void terminate();

  void propose(std::set<proposal_t> proposal);

private:
  /**
   * Enqueues a message to be broadcast
   * @param msg Message to broadcast
   */
  void broadcast(std::shared_ptr<Message> msg);

  /**
   * Enqueues a message to be sent to a specific destination
   * @param msg Message to be sent
   * @param dest Destination to send the message
   */
  void sendTo(std::shared_ptr<Message> msg, std::string dest);

  /**
   * Packet sending loop that continuously sends messages to the specified destination address while the run flag is set.
   */
  void send();

  /**
   * Packet listening loop that continuously listens for incoming messages and processes them while the run flag is set.
   */
  void listen();

  /**
   * Logger thread function that periodically writes log entries to the log file while the run flag is set.
   */
  void log();

  /**
   * Process lattice agreement (start new lattice agreement rounds)
   * TODO: Later can use mutliple threads that run this same loop to have multiple concurrent
   * lattice agreement instances all consuming proposals
   */
  void processLatticeAgreement();

private:
  proc_id_t id;
  std::unique_ptr<Logger> logger;
  std::atomic_bool runFlag;

  int node_socket;
  sockaddr_in node_addr;
  
  size_t nb_nodes;
  std::unordered_map<std::string, proc_id_t> others_id;
  std::unordered_map<std::string, std::unique_ptr<PerfectLink>> links;

  // Primitive implementations
  friend class LatticeAgreement; // Allow instances of LatticeAgreement to access attributes of Node
  LatticeAgreement lattice_agreement;

  ConcurrentDeque<std::set<proposal_t>> proposal_queue;

  // Worker threads
  std::thread sender_thread;
  std::thread listener_thread;
  std::thread logger_thread;
  std::thread lattice_agreement_processor_thread;
};