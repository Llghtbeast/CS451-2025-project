#include "node.hpp"

Node::Node(std::vector<Parser::Host> nodes, long unsigned int id, long unsigned int receiver_id, std::string outputPath)
  : id(id), output(outputPath) {
    Parser::Host node = nodes[id - 1];

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

    // Init message sequence number
    m_seq = 0;

    // Set up receiver address
    Parser::Host receiver = nodes[receiver_id - 1];
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(receiver.port);
    recv_addr.sin_addr.s_addr = receiver.ip;

    // Create sender id map
    for (Parser::Host n: nodes) {
      bool host_is_receiver = (n.id == receiver.id) & (n.ip == receiver.ip) & (n.port == receiver.port);
      if (!host_is_receiver) {
        std::ostringstream sender_ip_and_port;
        sender_ip_and_port << n.ip << ":" << n.portReadable();
        other_nodes_id[sender_ip_and_port.str()] = n.id;
      }
    }
  }

void Node::send() {
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

void Node::receive() {
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

  std::cout << "d " << other_nodes_id[sender_ip_and_port.str()] << " " << m_seq << "\n";

  output << "d " << other_nodes_id[sender_ip_and_port.str()] << " " << m_seq << "\n";
  return;
}

void Node::cleanup() {
  close(node_socket);
}
  
void Node::flushToOutput() {
  // Print termination to file and close resources
  output << "Process " << id << " terminated.\n";
  output.close();
}