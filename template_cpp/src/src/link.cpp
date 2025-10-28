#include "link.hpp"

/**
 * Constructor to initialize the Link with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
Link::Link(int socket, sockaddr_in source_addr, sockaddr_in dest_addr): socket(socket), source_addr(source_addr), dest_addr(dest_addr) {}

/**
 * Constructor to initialize the SenderLink with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
SenderLink::SenderLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr): Link(socket, source_addr, dest_addr)
{
  messageQueue = {};
}

/**
* Enqueues a message to be sent later.
* @param m_seq The sequence number of the message.
*/
uint32_t SenderLink::enqueueMessage()
{  
  // Increment message sequence number
  m_seq++;

  // Wait until there is space in the queue
  std::unique_lock<std::mutex> lock(queue_mutex);
  queue_cv.wait(lock, [&]() {
    return messageQueue.size() < maxQueueSize;
  });
  
  // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
  messageQueue.insert(messageQueue.end(), m_seq); 
  
  // Notify any waiting threads that a new message has been enqueued
  lock.unlock();

  return m_seq;
}

/**
 * Send the first enqueued messages.
 * @throws std::runtime_error if sending fails.
 */
void SenderLink::send()
{
  // Lock the queue while accessing it
  std::lock_guard<std::mutex> lock(queue_mutex);

  if (messageQueue.empty()) {
    return; // No messages to send
  }

  // set up for message iterator and send failure array
  std::set<uint32_t>::iterator it = messageQueue.begin();
  
  for (uint32_t i = 0; i < window_size && it != messageQueue.end(); i++, it++) {
    std::vector<uint32_t> msgs;

    uint8_t nb_msgs = 0;
    for (nb_msgs = 0; nb_msgs < Message::max_msgs && it != messageQueue.end(); nb_msgs++, it++) {
      msgs.push_back(*it);
    }

    // Transform message sequence number into big-endian for network transport
    Message message(MES, nb_msgs, msgs);
    std::cout << messageQueue.size() << " messages in queue. " << std::endl;
    std::cout << "Sending message with seq numbers: ";
    for (uint32_t seq: msgs) {
      std::cout << seq << " ";
    }
    std::cout << std::endl;
  
    // Send message to receiver
    if (sendto(socket, message.serialize(), message.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send message " << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      throw std::runtime_error(os.str());
    }
  }

}

/** 
 * Receive ACK from receiver and removed corresponding message from queue.
 * @param m_seq The sequence number of the acknowledged message.
*/
void SenderLink::receiveAck(std::vector<uint32_t> acked_messages)
{
  // Lock the queue while modifying it
  std::unique_lock<std::mutex> lock(queue_mutex);
  
  // Remove message from queue (correctly delivered at target)
  for (uint32_t seq: acked_messages) {
    messageQueue.erase(seq);
  }
  
  // Notify the waiting enqueuer thread that space is available in the queue
  lock.unlock();
  queue_cv.notify_one();
}

/**
 * Constructor to initialize the ReceiverLink with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
ReceiverLink::ReceiverLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr)
  : Link(socket, source_addr, dest_addr), deliveredMessages({})
{}

/**
 * Add message to delivered list, send ACK to sender, and return true if message was not already delivered. Otherwise, return false.
 * @param m_seq The sequence number of the message to be delivered.
 * @return True if the message was not already delivered, false otherwise.
 */
std::vector<bool> ReceiverLink::respond(Message message)
{
  // Update delivered message set and construct delivery status vector
  std::vector<bool> delivery_status;
  for (uint32_t m_seq: message.getSeqs()) {
    auto result = deliveredMessages.insert(m_seq);
    delivery_status.push_back(result.second); // true if inserted (not delivered before), false otherwise
  }

  // Remove leading delivered messages to avoid unbounded growth
  std::set<uint32_t>::iterator it = deliveredMessages.begin();
  uint32_t last_seq = *it - 1;
  while (it != deliveredMessages.end() && *it == last_seq + 1) {
    last_seq = *it;
    it = deliveredMessages.erase(it);
  }

  // Transform message to ack and serialize
  Message ackMsg = message.toAck();

  // Send ACK to sender
  if (sendto(socket, ackMsg.serialize(), ackMsg.serializedSize(), 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
    std::ostringstream os;
    os << "Failed to send ACK (errno: " << strerror(errno) << ") from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
    throw std::runtime_error(os.str());
  }
  return delivery_status;
}
