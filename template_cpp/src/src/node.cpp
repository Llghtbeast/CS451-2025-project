#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <netinet/in.h>
#include <unistd.h>

#include <parser.hpp>

class Node {
public:  
  Node(Parser::Host node, std::string outputPath): id(node.id), output(outputPath) {
    // Create IPv6 UDP socket 
    node_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (node_socket < 0) {
      std::ostringstream os;
      os << "Failed to create socket for host " << node.ipReadable() << ":" << node.portReadable();
      throw std::runtime_error(os.str());
    }

    // Set up receiver address
    memset(&node_addr, 0, sizeof(node_addr));
    node_addr.sin_family = AF_INET;
    node_addr.sin_port = htons(node.port);
    node_addr.sin_addr.s_addr = node.ip;

    // Bind the socket with the recevier address
    if (bind(node_socket, reinterpret_cast<const sockaddr*>(&node_addr), sizeof(node_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to bind socket to address " << node.ipReadable() << ":" << node.portReadable() << "\n";
      throw std::runtime_error(os.str());
    } else {
      std::cout << "Socket bound to address " << node.ipReadable() << ":" << node.portReadable() << "\n";
    }
  }

  void cleanup() {
    close(node_socket);
  }
  
  void flushToOutput() {
    // Print termination to file and close resources
    output << "Process " << id << " terminated.\n";
    output.close();
  }

  virtual ~Node() = default;

protected:
  long unsigned int id;

  std::ofstream output;

  int node_socket;
  sockaddr_in node_addr;
};