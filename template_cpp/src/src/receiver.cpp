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
  Receiver(const char * outputPath, Parser::Host receiver, std::vector<Parser::Host> hosts)
    : id{receiver.id} {
      // Init output file
      output = std::ofstream(outputPath);

      // Create sender id map
      for (Parser::Host host: hosts) {
        bool host_is_receiver = (host.id == receiver.id) & (host.ip == receiver.ip) & (host.port == receiver.port);
        if (!host_is_receiver) {
          std::stringstream sender_ip_and_port;
          sender_ip_and_port << host.ip << ":" << host.portReadable();
          sender_id[sender_ip_and_port.str()] = host.id;
        }
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
        os << "Failed to bind socket to address " << receiver.ipReadable() << ":" << receiver.portReadable() << "\n";
        throw std::runtime_error(os.str());
      } else {
        std::cout << "socket bound to address " << receiver.ipReadable() << ":" << receiver.portReadable() << "\n";
      }
    }

  void receive() {
    std::vector<char> buffer(sizeof(uint32_t));
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    std::cout << "waiting for message\n";

    if (recvfrom(recv_socket, buffer.data(), sizeof(uint32_t), 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len) < 0) {
      throw std::runtime_error("recvfrom failed ");
    }

    std::stringstream sender_ip_and_port;
    sender_ip_and_port << sender_addr.sin_addr.s_addr << ":" << sender_addr.sin_port;
    std::cout << "message received from " << sender_ip_and_port.str() << "\n";

    uint32_t network_byte_order_value;
    memcpy(&network_byte_order_value, buffer.data(), sizeof(uint32_t));
    uint32_t m_seq = ntohl(network_byte_order_value);

    std::cout << "d " << sender_id[sender_ip_and_port.str()] << " " << m_seq << "\n";

    output << "d " << sender_id[sender_ip_and_port.str()] << " " << m_seq << "\n";
    return;
  }

  void terminate() {
    // Print termination to file and close resources
    output << "Process " << id << " terminated.\n";
    output.close();
    close(recv_socket);
  }

private:
  long unsigned int id;

  std::ofstream output;

  std::unordered_map<std::string, long unsigned int> sender_id;

  int recv_socket;
  sockaddr_in recv_addr;
};