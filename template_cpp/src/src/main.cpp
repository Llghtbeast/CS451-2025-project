#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <memory>

#include "parser.hpp"
#include "sender.cpp"
#include "receiver.cpp"
#include <signal.h>

static Node* node = nullptr;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // Clean up resources
  std::cout << "Cleaning up resources.\n";
  node->cleanup();

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";
  node->flushToOutput();

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
  if (parser.id() == recv_id) {
    Receiver receiver(parser.outputPath(), hosts.at(recv_id-1), hosts);
    node = &receiver;

    std::cout << "Receiver created and waiting to receive\n";
    while (true) {
      receiver.receive();
    }
  } else {
    Sender sender(total_m, parser.outputPath(), hosts.at(parser.id()-1), hosts.at(recv_id-1));
    node = &sender;
    std::cout << "Sender created and starting to send\n";    

    for (size_t i = 0; i < total_m; i++) {
      sender.send();
    }
  }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
