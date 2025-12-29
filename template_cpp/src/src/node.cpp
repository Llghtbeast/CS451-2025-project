#include "node.hpp"

Node::Node(std::vector<Parser::Host> nodes, proc_id_t id, std::string outputPath, uint32_t ds)
  : id(id), 
    logger(std::make_unique<Logger>(outputPath)), 
    nb_nodes(nodes.size()),
    lattice_agreement(nodes.size(), ds, this)
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
      links[addr_hashable] = std::make_unique<PerfectLink>(node_socket, node_addr, n_addr);
    }
  }
}

Node::~Node()
{
  terminate();
  flushToOutput();
  cleanup();
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
  lattice_agreement_processor_thread = std::thread(&Node::processLatticeAgreement, this);
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
  if (lattice_agreement_processor_thread.joinable()) lattice_agreement_processor_thread.join();
}

void Node::propose(std::set<proposal_t> proposal)
{
  proposal_queue.push_back(proposal);
  std::this_thread::sleep_for(std::chrono::milliseconds(PROPOSAL_TIMEOUT_MS));
}

// Private methods:
void Node::broadcast(Message msg)
{
  // Enqueue message on all perfect links
  for (auto &pair: links) {
    // std::cout << "enqueuing message to " << pair.first << ": (" << origin_id << ", "<< seq << ")" << std::endl;
    pair.second->enqueueMessage(msg);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(BROADCAST_COOLDOWN_MS));
}

void Node::send()
{
  while (runFlag.load())
  {
    // std::cout << "Sending messages" << std::endl;
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
    std::array<char, Packet::max_serialized_size> buffer;
    sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
  
    // Sleeps until message received.
    ssize_t bytes_received = recvfrom(node_socket, buffer.data(), Packet::max_serialized_size, 0, reinterpret_cast<sockaddr *>(&sender_addr), &addr_len);
    if (bytes_received < 0) {
      std::cout << "recvfrom failed\n";
      continue;
      // throw std::runtime_error("recvfrom failed ");
    }
    else if (bytes_received == 0) {
      // std::cout << "Socket shutdown, stopping network packet processing." << std::endl;
      return; // Socket has been shut down
    }

    // Decode message
    std::string sender_ip_and_port = ipAddressToString(sender_addr);
    // std::cout << "message received from " << sender_ip_and_port << "" << std::endl;

    Packet pkt = Packet::deserialize(buffer.data());
    // pkt.displayPacket();
  
    // Process message through perfect link -> extract new received messages
    std::array<bool, MAX_MESSAGES_PER_PACKET> received_msgs = links[sender_ip_and_port]->receive(pkt);

    // if an ACK was received, so skip delivery processing
    if (pkt.getType() == MessageType::ACK) continue;

    std::array<Message, MAX_MESSAGES_PER_PACKET> msgs = pkt.getMessages();
    // Deliver message
    for (size_t i = 0; i < pkt.getNbMes(); i++) {
      // If message was already received, SKIP
      // std::cout << "received_msgs[" << i << "] = " << received_msgs[i] << "\n";
      if (!received_msgs[i]) continue;
            
      // If message has already been delivered, SKIP
      // if (delivered_messages[msgs[i].origin].contains(msgs[i].seq)) continue;
      
      // Log delivery
      // logger->logDelivery(msgs[i].origin, msgs[i].seq);

      // add message to delivered set
      // delivered_messages[msgs[i].origin].insert(msgs[i].seq);

      lattice_agreement.processMessage(msgs[i], sender_ip_and_port);
    }
  }
}

void Node::log() {
  // Listen while the run flag is set
  while (runFlag.load())
  {
    // Periodically write log entries to file
    // std::cout << "Logging" << std::endl;
    logger->write();
    std::this_thread::sleep_for(std::chrono::milliseconds(LOG_TIMEOUT));
  }
}

void Node::processLatticeAgreement()
{
  // Process while the run flag is set
  while (runFlag.load())
  {
    // Check queue for proposals, if empty, SLEEP and CONTINUE

    // Pop the first proposal from queue.
    auto proposal = proposal_queue.pop_front();

    // Propose this proposal to lattice agreement instance
    lattice_agreement.propose(proposal);

    // Wait for lattice_agreement instance to decide
    lattice_agreement.waitUntilDecided();

    // Reset lattice_agreement instance for next proposal round
    lattice_agreement.reset();
  }
}
