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

/**
 * Implementation of a network node that can send and receive messages.
 */
class Node {
public:  
  Node(std::vector<Parser::Host> nodes, long unsigned int id, long unsigned int receiver_id, std::string outputPath);
  void start();
  void enqueueMessage(sockaddr_in dest);
  void terminate();
  void cleanup();  
  void flushToOutput();

private:
  void send();
  void listen();
  void log();
  
private:
  long unsigned int id;
  long unsigned int recv_id;
  std::unique_ptr<Logger> logger;
  std::atomic_bool runFlag;

  int node_socket;
  sockaddr_in node_addr;

  std::unordered_map< std::string, uint64_t> others_id;
  std::unordered_map<std::string, std::unique_ptr<SenderLink>> sendLinks;
  std::unordered_map<std::string, std::unique_ptr<ReceiverLink>> recvLinks;
  sockaddr_in recv_addr; // TODO: remove for later implementations

  std::thread sender_thread;
  std::thread listener_thread;
  std::thread logger_thread;
};