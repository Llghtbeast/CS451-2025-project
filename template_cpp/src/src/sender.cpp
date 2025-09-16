#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <stdint.h>
#include <iostream>
#include <fstream>

#include <parser.hpp>

class Sender {
public:
  Sender(uint32_t total_mes, const char * outputPath, Parser::Host &sender, Parser::Host &receiver)
    : id{sender.id}, total_m{total_m} {
      // Init message sequence number
      m_seq = 0;

      // Init output file
      output = std::ofstream(outputPath);

      // Create IPv6 UDP socket 
      send_socket = socket(AF_INET, SOCK_DGRAM, 0);
      if (send_socket < 0) {
        std::ostringstream os;
        os << "Failed to create socket for host " << sender.ipReadable() << ":" << sender.portReadable();
        throw std::runtime_error(os.str());
      }

      // Set up sender address
      memset(&send_addr, 0, sizeof(send_addr));
      send_addr.sin_family = AF_INET;
      send_addr.sin_port = htons(sender.port);
      send_addr.sin_addr.s_addr = sender.ip;

      // Bind the socket with the server address
      if (bind(send_socket, reinterpret_cast<const sockaddr*>(&send_addr), sizeof(send_addr)) < 0) {
        std::ostringstream os;
        os << "Failed to bind socket to address " << sender.ipReadable() << ":" << sender.portReadable();
        throw std::runtime_error(os.str());
      }

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
    output << "b " + m_seq;

    // Send message to receiver
    sendto(send_socket, buffer.data(), sizeof(uint32_t), 0, reinterpret_cast<const sockaddr *>(&recv_addr), sizeof(recv_addr));
  }

  void terminate() {
    // Print termination to file and close resources
    output << "Process " << id << " terminated.";
    output.close();
    close(send_socket);
  }

private:
  uint32_t id;

  uint32_t m_seq;
  const uint32_t total_m;
  std::ofstream output;

  int send_socket;
  sockaddr_in send_addr;
  sockaddr_in recv_addr;
};