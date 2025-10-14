#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>

#include <parser.hpp>
#include "node.cpp"

class Receiver: public Node {
public:
  Receiver(const char * outputPath, Parser::Host receiver, std::vector<Parser::Host> hosts)
    : Node(receiver, outputPath) {

      // Create sender id map
      for (Parser::Host host: hosts) {
        bool host_is_receiver = (host.id == receiver.id) & (host.ip == receiver.ip) & (host.port == receiver.port);
        if (!host_is_receiver) {
          std::stringstream sender_ip_and_port;
          sender_ip_and_port << host.ip << ":" << host.portReadable();
          sender_id[sender_ip_and_port.str()] = host.id;
        }
      }
    }

  void receive() {
    std::vector<char> buffer(sizeof(uint32_t));
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    std::cout << "waiting for message\n";

    if (recvfrom(node_socket, buffer.data(), sizeof(uint32_t), 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len) < 0) {
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

private:
  std::unordered_map<std::string, long unsigned int> sender_id;
};