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

#include "parser.hpp"
#include "link.hpp"
#include "helper.hpp"

/**
 * Implementation of a network node that can send and receive messages.
 */
class Node {
public:  
  Node(std::vector<Parser::Host> nodes, long unsigned int id, long unsigned int receiver_id, std::string outputPath);
  void enqueueMessage(sockaddr_in dest, std::string m = "");
  void sendAndListen();
  void cleanup();  
  void flushToOutput();

protected:
  long unsigned int id;
  long unsigned int recv_id;
  std::ofstream output;

  int node_socket;
  sockaddr_in node_addr;

  uint32_t m_seq;
  std::unordered_map< std::string, uint64_t> others_id;
  std::unordered_map<std::string, SenderLink *> sendLinks;
  std::unordered_map<std::string, ReceiverLink *> recvLinks;
  sockaddr_in recv_addr; // TODO: remove for later implementations
};