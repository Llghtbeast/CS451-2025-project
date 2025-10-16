#include "node.hpp"

/**
 * Constructor to initialize the network node with its neighbors, its ID, the network's receiver ID, and the file output path. It sets up the UDP socket and binds it to the node's address.
 * @param nodes A vector of all nodes in the network.
 * @param id The unique identifier for this node.
 * @param receiver_id The unique identifier for the network's receiver node.
 * @param outputPath The path to the output file where messages will be logged.
 */
Node::Node(std::vector<Parser::Host> nodes, long unsigned int id, long unsigned int receiver_id, std::string outputPath)
  : id(id), recv_id(receiver_id), output(outputPath) 
  {
    Parser::Host node = nodes[id - 1];

    // Create IPv6 UDP socket 
    node_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (node_socket < 0) {
      std::ostringstream os;
      os << "Failed to create socket for host " << node.ipReadable() << ":" << node.portReadable();
      throw std::runtime_error(os.str());
    }

    // Set up receiver address
    setupIpAddress(node_addr, node);

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
    setupIpAddress(recv_addr, receiver);

    // Create sender id map
    for (Parser::Host n: nodes) {
      if (n.id != id) {
        // Create node address and map to node id.
        sockaddr_in n_addr = {0};
        setupIpAddress(n_addr, n);
        std::string addr_hashable = ipAddressToString(n_addr);
        others_id[addr_hashable] = n.id;

        // Create network links
        SenderLink sendLink(node_socket, node_addr, n_addr);
        ReceiverLink recvLink(node_socket, node_addr, n_addr);
        sendLinks[addr_hashable] = &sendLink;
        recvLinks[addr_hashable] = &recvLink;
      }
    }
  }

/**
 * Set up address from host
 */
void setupIpAddress(sockaddr_in addr, Parser::Host host)
{
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(host.port);
  addr.sin_addr.s_addr = host.ip;
}

/**
 * Generate hashable string of IP address of a node
 */
std::string ipAddressToString(sockaddr_in addr)
{
  std::ostringstream sender_ip_and_port;
  sender_ip_and_port << addr.sin_addr.s_addr << ":" << addr.sin_port;
  return sender_ip_and_port.str();
}

/**
 * Enqueue new message
 */
void Node::enqueueMessage(std::string m = "", sockaddr_in dest)
{
  if (m != "") {
    throw std::runtime_error("Non-empty message is not implemented yet");
  }

  // Increment message sequence number
  m_seq++;

  // Transform message sequence number into big-endian for network transport
  std::string dest_str = ipAddressToString(dest);
  sendLinks[dest_str]->enqueueMessage(m_seq);

  // Write send message to file
  output << "b " << m_seq << "\n";
}

void Node::sendAndListen()
{
  // TODO: choose which links to send messages on.
  std::cout << "Sending a few messages.\n";
  for (auto pair: sendLinks) {
    pair.second->send();
  }
  
  // Listen for messages
  std::cout << "Listening for message\n";
  std::vector<char> buffer(sizeof(uint32_t));
  sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);

  // TODO: do logic for message reception
  if (recvfrom(node_socket, buffer.data(), , 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len) < 0) {
    throw std::runtime_error("recvfrom failed ");
  }

  std::string sender_ip_and_port = ipAddressToString(sender_addr);
  std::cout << "message received from " << sender_ip_and_port << "\n";

  uint32_t network_byte_order_value;
  memcpy(&network_byte_order_value, buffer.data(), sizeof(uint32_t));
  uint32_t m_seq = ntohl(network_byte_order_value);

  std::cout << "d " << others_id[sender_ip_and_port] << " " << m_seq << "\n";

  output << "d " << others_id[sender_ip_and_port] << " " << m_seq << "\n";
  return;
}

void Node::cleanup() 
{
  close(node_socket);
}
  
void Node::flushToOutput() 
{
  // Print termination to file and close resources
  output << "Process " << id << " terminated.\n";
  output.close();
}