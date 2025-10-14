#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <stdint.h>
#include <iostream>
#include <fstream>

#include <parser.hpp>
#include "node.cpp"

class Sender: public Node {
public:
  Sender(uint32_t total_mes, const char * outputPath, Parser::Host sender, Parser::Host receiver)
    : Node(sender, outputPath), total_m(total_mes) {
      // Init message sequence number
      m_seq = 0;

      // Set up receiver address
      memset(&recv_addr, 0, sizeof(recv_addr));
      recv_addr.sin_family = AF_INET;
      recv_addr.sin_port = htons(receiver.port);
      recv_addr.sin_addr.s_addr = receiver.ip;
  }

  void send() {
    // Increment message sequence number
    m_seq++;

    // Transform message sequence number into big-endian for network transport
    std::vector<char> buffer(sizeof(m_seq));
    uint32_t network_byte_order_m_seq = htonl(m_seq);
    memcpy(buffer.data(), &network_byte_order_m_seq, sizeof(uint32_t));
  
    // Write send message to file
    output << "b " << m_seq << "\n";

    // Send message to receiver
    sendto(node_socket, buffer.data(), sizeof(uint32_t), 0, reinterpret_cast<const sockaddr *>(&recv_addr), sizeof(recv_addr));
  }

private:
  uint32_t m_seq;
  const uint32_t total_m;

  sockaddr_in recv_addr;
};