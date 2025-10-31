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

static Node* p_node = nullptr;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  
  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";
  p_node->terminate();
  
  // write/flush output file if necessary
  std::cout << "Writing output.\n";
  p_node->flushToOutput();

  // Clean up resources
  std::cout << "Cleaning up resources.\n";
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

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";


  std::cout << "Doing some initialization...\n\n";

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
  uint32_t total_m, recv_id;
  if (!(iss >> total_m >> recv_id)) {
    throw std::invalid_argument("Error parsing integers from config file");
  }
  
  std::cout << "Broadcasting and delivering messages...\n\n";

  // Create senders and receiver
  Node node(hosts, parser.id(), recv_id, parser.outputPath());
  p_node = &node;
  
  
  // Start node (sending and listening)
  std::thread node_thread(&Node::start, &node);
  node_thread.detach();
  
  if (parser.id() == recv_id) {
    std::cout << "Receiver created and waiting to receive\n";
  } else {
    std::cout << "Sender created and starting to send " << total_m << " messages\n"; 
    
    // Start clock to measure Sender execution time
    auto start_time = std::chrono::high_resolution_clock::now();
        
    // Start enqueueing messages if this is a sender
    for (size_t i = 0; i < total_m;) {
      // Try to enqueue message, if queue is full yield to other threads and retry
      node.enqueueMessage(setupIpAddress(parser.hosts()[recv_id - 1]));
      i++;
    }
    std::cout << "All messages enqueued.\n\n";
    node.allMessagesEnqueued(setupIpAddress(parser.hosts()[recv_id - 1]));
    node.finished(setupIpAddress(parser.hosts()[recv_id - 1]));
    
    auto stop_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop_time - start_time);
    std::cout << "\nSender execution duration: " << duration.count() << " milliseconds\n\n";
  }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
