#include <sys/socket.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <netinet/in.h>
#include <unistd.h>

#include "parser.hpp"

/**
 * Implementation of a network node that can send and receive messages.
 */
class Node {
public:  
  Node(std::vector<Parser::Host> nodes, long unsigned int id, long unsigned int receiver_id, std::string outputPath);
  void send();
  void receive();
  void cleanup();  
  void flushToOutput();

protected:
  long unsigned int id;
  std::ofstream output;

  int node_socket;
  sockaddr_in node_addr;

  uint32_t m_seq;
  std::unordered_map<std::string, long unsigned int> other_nodes_id;
  sockaddr_in recv_addr;
};