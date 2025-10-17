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
    node_addr = setupIpAddress(node);

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
    recv_addr = setupIpAddress(receiver);

    // Create sender id map
    for (Parser::Host n: nodes) {
      if (n.id != id) {
        // Create node address and map to node id.
        sockaddr_in n_addr = setupIpAddress(n);
        std::string addr_hashable = ipAddressToString(n_addr);
        others_id[addr_hashable] = n.id;

        // Create network links
        sendLinks[addr_hashable] = std::make_unique<SenderLink>(node_socket, node_addr, n_addr);
        recvLinks[addr_hashable] = std::make_unique<ReceiverLink>(node_socket, node_addr, n_addr);
      }
    }
  }

/**
 * Enqueue new message
 */
void Node::enqueueMessage(sockaddr_in dest, std::string m)
{
  if (m != "") {
    throw std::runtime_error("Non-empty message is not implemented yet");
  }

  // Increment message sequence number
  m_seq++;

  // Transform message sequence number into big-endian for network transport
  std::string dest_str = ipAddressToString(dest);
  std::cout << "test\n";
  sendLinks[dest_str]->enqueueMessage(m_seq);

  // Write enqueued messages to file
  output << "b " << m_seq << "\n";
}

void Node::sendAndListen()
{
  // TODO: choose which links to send messages on.
  std::cout << "Sending a few messages.\n";
  for (auto &pair: sendLinks) {
    // Send messages enqueued on each sender link
    pair.second->send();
  }
  
  // Listen for messages
  std::cout << "Listening for message\n";
  std::vector<char> buffer(sizeof(uint32_t));
  sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);

  // Check for incoming messages
  for (size_t i = 0; i < Link::window_size; i++)
  {
    if (recvfrom(node_socket, buffer.data(), Link::buffer_size, 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len) < 0) {
      throw std::runtime_error("recvfrom failed ");
    }
    std::string sender_ip_and_port = ipAddressToString(sender_addr);
    std::cout << "message received from " << sender_ip_and_port << "\n";
  
    // Decode message
    std::pair<uint32_t, uint8_t> decoded = Link::decodeMessage(buffer);
    uint32_t m_seq = decoded.first;
    uint8_t message_type = decoded.second;

    if (message_type == MES) {
      // Deliver message to receiver link
      bool delivered = recvLinks[sender_ip_and_port]->deliver(m_seq);
      if (delivered) {
        std::cout << "d " << others_id[sender_ip_and_port] << " " << m_seq << "\n";
        output << "d " << others_id[sender_ip_and_port] << " " << m_seq << "\n";
      }
    } else if (message_type == ACK) {
      // Process ACK on sender link
      sendLinks[sender_ip_and_port]->receiveAck(m_seq);
      std::cout << "ACK " << m_seq << " processed.\n";
    } else {
      throw std::runtime_error("Unknown message type received.");
    }
  }

  // Wait to not congest the network
  std::this_thread::sleep_for(std::chrono::microseconds(100));
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