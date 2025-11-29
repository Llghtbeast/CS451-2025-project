#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <memory>
#include <signal.h>

#include "parser.hpp"
#include "node.hpp"
#include "helper.hpp"
#include "globals.hpp"

static Node* p_node = nullptr;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  
  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing." << std::endl;
  p_node->terminate();
  
  // write/flush output file if necessary
  std::cout << "Writing output." << std::endl;
  p_node->flushToOutput();

  // Clean up resources
  std::cout << "Cleaning up resources." << std::endl;
  p_node->cleanup();

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "" << std::endl;
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n" << std::endl;

  std::cout << "My ID: " << parser.id() << "\n" << std::endl;

  std::cout << "List of resolved hosts is:" << std::endl;
  std::cout << "==========================" << std::endl;
  auto hosts = parser.hosts();
  for (auto &host : hosts) {
    std::cout << host.id << "" << std::endl;
    std::cout << "Human-readable IP: " << host.ipReadable() << "" << std::endl;
    std::cout << "Machine-readable IP: " << host.ip << "" << std::endl;
    std::cout << "Human-readbale Port: " << host.portReadable() << "" << std::endl;
    std::cout << "Machine-readbale Port: " << host.port << "" << std::endl;
    std::cout << "" << std::endl;
  }
  std::cout << "" << std::endl;

  std::cout << "Path to output:" << std::endl;
  std::cout << "===============" << std::endl;
  std::cout << parser.outputPath() << "\n" << std::endl;

  std::cout << "Path to config:" << std::endl;
  std::cout << "===============" << std::endl;
  std::cout << parser.configPath() << "\n" << std::endl;


  std::cout << "Doing some initialization...\n" << std::endl;

  // Open config file
  std::ifstream configFile(parser.configPath());
  if (!configFile.is_open()) {
    std::ostringstream os;
    os << "`" << parser.configPath() << "` does not exist";
    throw std::invalid_argument(os.str());
  }

  // Extract first line of config file
  std::string config;
  if (!std::getline(configFile, config)) {
    std::ostringstream os;
    os << "`" << parser.configPath() << "` file empty or error handling file";
    throw std::invalid_argument(os.str());
  }

  // Extract total number of messages sent by senders and id of receiver
  std::istringstream iss(config);
  msg_seq_t total_m;
  if (!(iss >> total_m)) {
    throw std::invalid_argument("Error parsing integers from config file");
  }
  
  std::cout << "Broadcasting and delivering messages...\n" << std::endl;

  // Create node
  Node node(hosts, parser.id(), parser.outputPath());
  p_node = &node;
  
  // Start node (sending and listening)
  std::thread node_thread(&Node::start, &node);
  node_thread.detach();
  
  std::cout << "Node started successfully.\n" << std::endl;
    
  // Start clock to measure Sender execution time
  auto start_time = std::chrono::high_resolution_clock::now();
      
  // Start enqueueing messages if this is a sender
  for (size_t i = 0; i < total_m; i++) {
    // Try to broadcast message with this node's id as origin, if queue is full yield to other threads and retry
    node.broadcast(parser.id());
  }

  std::cout << "All messages enqueued.\n" << std::endl;
  // node.allMessagesEnqueued(setupIpAddress(parser.hosts()[recv_id - 1]));
  // node.finished(setupIpAddress(parser.hosts()[recv_id - 1]));
  
  // auto stop_time = std::chrono::high_resolution_clock::now();
  // auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop_time - start_time);
  // std::cout << "Node execution duration: " << duration.count() << " milliseconds\n" << std::endl;

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
