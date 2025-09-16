#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>

#include <parser.hpp>

class Receiver {
public:
  Receiver(const char * outputPath, Parser::Host &receiver, std::vector<Parser::Host&> senders) {
    // Init output file
    output = std::ofstream(outputPath);

    // Create sender id map
    for (Parser::Host sender: senders) {
      std::string sender_ip_and_port << sender.ip <<
      sender_id[]
    }

    // Create IPv6 UDP socket 
    recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_socket < 0) {
      std::ostringstream os;
      os << "Failed to create socket for host " << receiver.ipReadable() << ":" << receiver.portReadable();
      throw std::runtime_error(os.str());
    }

    // Set up receiver address
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(receiver.port);
    recv_addr.sin_addr.s_addr = receiver.ip;

    // Bind the socket with the recevier address
    if (bind(recv_socket, reinterpret_cast<const sockaddr*>(&recv_addr), sizeof(recv_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to bind socket to address " << receiver.ipReadable() << ":" << receiver.portReadable();
      throw std::runtime_error(os.str());
    }
  }

  void receive() {
    std::vector<char> buffer(sizeof(uint32_t));
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    if (recvfrom(recv_socket, buffer.data(), sizeof(uint32_t), 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len) < 0) {
      throw std::runtime_error("recvfrom failed ");
    }

    output << "d " << 
  }

  void terminate() {
    // Print termination to file and close resources
    output << "Process " << id << " terminated.";
    output.close();
    close(recv_socket);
  }

private:
  uint32_t id;

  std::ofstream output;

  std::unordered_map<std::string, int> sender_id;

  int recv_socket;
  sockaddr_in recv_addr;
};