#include "node.hpp"

Node::Node(std::vector<Parser::Host> nodes, proc_id_t id, std::string outputPath)
  : id(id), logger(std::make_unique<Logger>(outputPath)), nb_nodes(nodes.size()), 
    pending_messages(), delivered_messages(), acked_by()
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
      os << "Failed to bind socket to address " << node.ipReadable() << ":" << node.portReadable() << "" << std::endl;
      throw std::runtime_error(os.str());
    } else {
      // std::cout << "Socket bound to address " << node.ipReadable() << ":" << node.portReadable() << "" << std::endl;
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

      // Initialize delivered messages set for each node
      pending_messages.try_emplace(n.id);
      delivered_messages.try_emplace(n.id);
      acked_by.try_emplace(n.id);
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
  links[dest_str]->enqueueMessage(id, ++m_seq);
  logger->logBroadcast(m_seq);
}

void Node::broadcast(proc_id_t origin_id, msg_seq_t seq)
{
  // Check if broadcasting node is this node
  if (seq == 0) assert(origin_id == id);
  if (origin_id == id) {
    // Increment message sequence number
    m_seq++;
    seq = m_seq;
  }

  // Enqueue message on all perfect links
  for (auto &pair: links) {
    // std::cout << "enqueuing message to " << pair.first << ": (" << origin_id << ", "<< seq << ")" << std::endl;
    pair.second->enqueueMessage(origin_id, seq);
  }
  
  // If this node is sender, log message
  if (origin_id == id) {
    // Log broadcast, add to pending messages and acked_by structures
    logger->logBroadcast(m_seq);
    pending_messages[origin_id].insert(seq);
    acked_by[origin_id].add_to_mapped_set(seq, origin_id);
  }
}

bool Node::can_deliver(proc_id_t origin_id, msg_seq_t seq)
{
  return acked_by[origin_id].mapped_set_size(seq) > nb_nodes / 2;
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
    // std::cout << "Listening for message" << std::endl;
    std::vector<char> buffer(Message::max_size);
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
  
    // Sleeps until message received.
    ssize_t bytes_received = recvfrom(node_socket, buffer.data(), Message::max_size, 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len);
    if (bytes_received < 0) {
      throw std::runtime_error("recvfrom failed ");
    }
    else if (bytes_received == 0) {
      // std::cout << "Socket shutdown, stopping network packet processing." << std::endl;
      return; // Socket has been shut down
    }

    // Decode message
    std::string sender_ip_and_port = ipAddressToString(sender_addr);
    // std::cout << "message received from " << sender_ip_and_port << "" << std::endl;

    Message msg = Message::deserialize(buffer.data());
    Message::displayMessage(msg);
  
    // Process message through perfect link -> extract new received messages
    std::vector<bool> received_msgs = links[sender_ip_and_port]->receive(msg);
    std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> msg_payload = msg.getPayloads();

    // If received_msgs is empty, it means an ACK was received, so skip delivery processing
    if (received_msgs.empty()) continue;

    // Deliver message
    for (size_t i = 0; i < msg.getNbMes(); i++) {
      // If message was already received, SKIP
      if (!received_msgs[i]) continue;
      
      const auto& [_, origin_id, seq] = msg_payload[i];
      
      // If message has already been delivered, SKIP
      if (delivered_messages[origin_id].contains(seq)) continue;
      
      // Update acked_by structure
      acked_by[origin_id].add_to_mapped_set(seq, others_id[sender_ip_and_port]); // Ack by sender of message
      
      // Check if message is already in pending set
      if (!pending_messages[origin_id].contains(seq)) {;
        // std::cout << "Adding (" << origin_id << ", " << seq << ") to pending list" << std::endl;
        // Add message to pending messages and acked_by structures
        pending_messages[origin_id].insert(seq);
        acked_by[origin_id].add_to_mapped_set(seq, id); // Ack by self
        broadcast(origin_id, seq);
      }

      // Print received message and all data structures
      std::cout << "\n=== Message State ===" << std::endl;
      std::cout << "Received message (" << origin_id << ", " << seq << "), from: " << others_id[sender_ip_and_port] << std::endl;
      
      // Print pending messages
      std::cout << "\nPending messages:" << std::endl;
      for (const auto& [origin, pending_set]: pending_messages) {
        std::vector<msg_seq_t> pending_snapshot = pending_set.snapshot();
        std::cout << "  Origin " << origin << ": ";
        for (msg_seq_t m : pending_snapshot) {
          std::cout << m << " ";
        }
        std::cout << std::endl;
      }
      
      // Print delivered messages
      std::cout << "\nDelivered messages:" << std::endl;
      for (const auto& [origin, delivered_set]: delivered_messages) {
        std::cout << "  Origin " << origin << ": ";
        delivered_set.display();
      }
      
      // Print acked_by map
      std::cout << "\nAcked by:" << std::endl;
      for (const auto& [origin, acked_map]: acked_by) {
        std::cout << "  Origin " << origin << ":" << std::endl;
        auto acked_snapshot = acked_map.snapshot();
        for (const auto& [msg_seq, ack_set] : acked_snapshot) {
          std::cout << "    Seq " << msg_seq << ": {";
          for (proc_id_t acker : ack_set) {
            std::cout << acker << " ";
          }
          std::cout << "}" << std::endl;
        }
      }

      if (can_deliver(origin_id, seq)) std::cout << "Delivering message: (" << origin_id << ", " << seq << ")" << std::endl;
      std::cout << "==================\n" << std::endl;

      // Check if message can be delivered
      if (can_deliver(origin_id, seq)) {
        // std::cout << "Delivering (" << origin_id << ", " << seq << ")" << std::endl;
        // Log delivery
        logger->logDelivery(origin_id, seq);
        // Remove from pending messages
        pending_messages[origin_id].erase(seq);
        acked_by[origin_id].erase(seq);
        delivered_messages[origin_id].insert(seq);
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
  // std::cout << std::endl;

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