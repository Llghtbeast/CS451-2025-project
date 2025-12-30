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

  // Extract lattice agreement parameters (p, vs, ds)
  std::istringstream iss(config);
  prop_nb_t shots; uint32_t vs; uint32_t ds;
  if (!(iss >> shots >> vs >> ds)) {
    throw std::invalid_argument("Error parsing integers from config file");
  }
  
  // Create node
  std::cout << "Creating nodes for lattice agreement (p=" << shots << ", vs=" << vs << ", ds=" << ds << ")\n" << std::endl;
  Node node(hosts, parser.id(), parser.outputPath(), ds);
  p_node = &node;

  try {
    // Start node (sending, listening and logging)
    node.start();
    std::cout << "Node started successfully.\n" << std::endl;
      
    // Start clock to measure Sender execution time
    // auto start_time = std::chrono::high_resolution_clock::now();
        
    // Start enqueueing messages if this is a sender
    for (size_t i = 0; i < shots; i++) {
      std::string line;
      if (!std::getline(configFile, line)) {
        std::ostringstream os;
        os << "`" << parser.configPath() << "` file empty or error handling file";
        throw std::invalid_argument(os.str());
      }
      std::istringstream line_stream(line);

      // Create proposal set
      std::set<proposal_t> proposal;
      proposal_t element;
      while (line_stream >> element)
      {
        proposal.insert(element); 
      }
      
      // Propose to node
      node.propose(std::move(proposal));
    }
  
    std::cout << "All proposals enqueued.\n" << std::endl;
    
    // node.finished(setupIpAddress(parser.hosts()[recv_id - 1]));
    
    // auto stop_time = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop_time - start_time);
    // std::cout << "Node execution duration: " << duration.count() << " milliseconds\n" << std::endl;
  
    // After a process finishes broadcasting,
    // it waits forever for the delivery of messages.
    while (true) {
      std::this_thread::sleep_for(std::chrono::hours(1));
    }
  }
  catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
    stop(0);
  }  
  
  return 0;
}
