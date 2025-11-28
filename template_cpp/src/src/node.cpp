#include "node.hpp"

Node::Node(std::vector<Parser::Host> nodes, proc_id_t id, std::string outputPath)
  : id(id), logger(std::make_unique<Logger>(outputPath)), nb_nodes(nodes.size()), pending_messages(MAX_QUEUE_SIZE), delivered_messages(INITIAL_SLIDING_SET_PREFIX), acked_by({})
  {
    // Initialize run flag
    runFlag.store(false);

    // Extract this node's information
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

    // Create sender id map
    for (Parser::Host n: nodes) {
      if (n.id != id) {
        // Create node address and map to node id.
        sockaddr_in n_addr = setupIpAddress(n);
        std::string addr_hashable = ipAddressToString(n_addr);
        others_id[addr_hashable] = n.id;

        // Create network links
        links[addr_hashable] = std::make_unique<PerfectLink>(node_socket, id, node_addr, n_addr);
      }
    }
  }

void Node::start()
{
  // avoid starting multiple times
  if (runFlag.load()) {
    return;
  }

  runFlag.store(true);

  // start worker threads bound to this instance
  sender_thread = std::thread(&Node::send, this);
  listener_thread = std::thread(&Node::listen, this);
  logger_thread = std::thread(&Node::log, this);
}

void Node::enqueueMessage(sockaddr_in dest)
{
  std::string dest_str = ipAddressToString(dest);
  links[dest_str]->enqueueMessage(++m_seq);
  logger->logBroadcast(m_seq);
}

void Node::broadcast()
{
  // Increment message sequence number
  m_seq++;

  // Enqueue message on all perfect links
  for (auto &pair: links) {
    pair.second->enqueueMessage(m_seq);
  }
  
  // Log broadcast, add to pending messages and acked_by structures
  // logger->logBroadcast(m_seq);
  // pending_messages.insert(m_seq);
  // acked_by[m_seq] = {id};
}

void Node::send()
{
  while (runFlag.load())
  {
    // Try sending messages from all sender links
    for (auto &pair: links) {
      // Send messages enqueued on each sender link
      pair.second->send();
    }

    // Sleep for a short duration to avoid busy-waiting (waiting for messages to be enqueued)
    // For optimal performance, could try to design a congestion control algorithm to adjust sending rate based on network conditions
    std::this_thread::sleep_for(std::chrono::milliseconds(SEND_TIMEOUT_MS));
  }
}

void Node::listen()
{
  // Listen while the run flag is set
  while (runFlag.load())
  {
    // Prepare buffer to receive message
    std::cout << "Listening for message\n";
    std::vector<char> buffer(Message::max_size);
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
  
    // Sleeps until message received.
    ssize_t bytes_received = recvfrom(node_socket, buffer.data(), Message::max_size, 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len);
    if (bytes_received < 0) {
      throw std::runtime_error("recvfrom failed ");
    }
    else if (bytes_received == 0) {
      std::cout << "Socket shutdown, stopping network packet processing.\n";
      return; // Socket has been shut down
    }

    // Decode message
    Message msg = Message::deserialize(buffer.data());
    
    std::string sender_ip_and_port = ipAddressToString(sender_addr);
    std::cout << "message received from " << sender_ip_and_port << "\n";
  
    // Process message through perfect link -> extract new received messages
    std::vector<bool> received_msgs = links[sender_ip_and_port]->receive(msg);

    // Deliver message
    if (!received_msgs.empty()) {
      for (size_t i = 0; i < msg.getNbMes(); i++) {
        if (received_msgs[i]) {
          msg_seq_t seq = msg.getSeqs()[i];
          logger->logDelivery(msg.getOriginId(), seq);
        }
      }
    }
  }
}

void Node::log() {
  // Listen while the run flag is set
  while (runFlag.load())
  {
    // Periodically write log entries to file
    logger->write();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void Node::terminate()
{
  // Stop the node's sending and listening threads (finish execution and exit)
  runFlag.store(false);

  // flush standard output to debug
  std::cout << std::endl;

  // unblock recvfrom if it's blocked
  ::shutdown(node_socket, SHUT_RDWR);

  // join threads (wait for loops to exit)
  if (sender_thread.joinable()) sender_thread.join();
  if (listener_thread.joinable()) listener_thread.join();
  if (logger_thread.joinable()) logger_thread.join();
}

void Node::allMessagesEnqueued(sockaddr_in dest) {
  std::string dest_str = ipAddressToString(dest);
  links[dest_str]->allMessagesEnqueued();
}

void Node::finished(sockaddr_in dest) {
  std::string dest_str = ipAddressToString(dest);
  links[dest_str]->finished();
}

void Node::cleanup() 
{
  close(node_socket);
  logger->cleanup();
}

void Node::flushToOutput() 
{
  // Flush output stream to ensure all received messages are logged.
  logger->write();
  logger->flush();
}