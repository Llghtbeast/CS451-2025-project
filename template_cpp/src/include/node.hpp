#include <sys/socket.h>
#include <string.h>
#include <cstring>
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
  /**
   * Constructor to initialize the network node with its neighbors, its ID, the network's receiver ID, and the file output path. It sets up the UDP socket and binds it to the node's address.
   * @param nodes A vector of all nodes in the network.
   * @param id The unique identifier for this node.
   * @param receiver_id The unique identifier for the network's receiver node.
   * @param outputPath The path to the output file where messages will be logged.
   */
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