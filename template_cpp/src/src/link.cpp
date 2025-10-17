#include "link.hpp"

/**
 * Constructor to initialize the Link with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
Link::Link(int socket, sockaddr_in source_addr, sockaddr_in dest_addr): socket(socket), source_addr(source_addr), dest_addr(dest_addr) {}

/**
 * Encodes a message sequence number into a byte vector for transmission.
 * @param m_seq The sequence number of the message.
 * @param is_ack A boolean indicating if the message is an acknowledgment.
 * @return A vector of characters representing the encoded message.
 */
std::vector<char> Link::encodeMessage(uint32_t m_seq, uint8_t message_type)
{
  std::vector<char> buffer(Link::buffer_size);
  buffer[0] = static_cast<char>(message_type);
  uint32_t network_byte_order_m_seq = htonl(m_seq);
  memcpy(buffer.data() + 1, &network_byte_order_m_seq, sizeof(network_byte_order_m_seq));
  return buffer;
}

/**
 * Decodes a received message from a byte vector.
 * @param buffer The vector of characters representing the received message.
 * @return A pair containing the message sequence number and the message type.
 * @throws std::runtime_error if the buffer is too small.
 */
std::pair<uint32_t, uint8_t> Link::decodeMessage(const std::vector<char>& buffer)
{
  if (buffer.size() < Link::buffer_size) {
    throw std::runtime_error("Link::decodeMessage: buffer too small");
  }

  uint8_t message_type = static_cast<uint8_t>(buffer[0]);

  uint32_t network_byte_order_m_seq = 0;
  memcpy(&network_byte_order_m_seq, buffer.data() + 1, sizeof(network_byte_order_m_seq));
  uint32_t m_seq = ntohl(network_byte_order_m_seq);

  return {m_seq, message_type};
}

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
* @throws std::runtime_error if message queue grows too large.
*/
void SenderLink::enqueueMessage(uint32_t m_seq)
{  
  if (!messageQueue.empty() & (messageQueue.size() >= 1'000'000)) {
    std::ostringstream os;
    os << "Sender " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " 's message queue is growing too large ";
    throw std::runtime_error(os.str());
  }
  std::cout << messageQueue.size() << "\n";
  messageQueue.insert(messageQueue.end(), m_seq); // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
}

/**
 * Send the first enqueued messages.
 * @throws std::runtime_error if sending fails.
 */
void SenderLink::send()
{
  // set up for message iterator and send failure array
  std::set<uint32_t>::iterator it = messageQueue.begin();
  std::vector<uint32_t> send_failure;

  for (uint32_t i = 0; i < window_size && it != messageQueue.end(); i++, it++) {
    uint32_t m_seq = *it;

    // Transform message sequence number into big-endian for network transport
    std::vector<char> buffer = encodeMessage(m_seq, MES);

    // Send message to receiver
    // TODO: group multiple messages into one UDP packet
    if (sendto(socket, buffer.data(), Link::buffer_size, 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      send_failure.push_back(m_seq);
    }
  }

  if (!send_failure.empty()) {
    std::ostringstream os;
    os << "Failed to send message ";
    for (uint32_t m_seq: send_failure) { 
      os << m_seq << ", ";
    }
    os << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
    throw std::runtime_error(os.str());
  }
}

/** 
 * Receive ACK from receiver and removed corresponding message from queue.
 * @param m_seq The sequence number of the acknowledged message.
*/
void SenderLink::receiveAck(uint32_t m_seq)
{
  messageQueue.erase(m_seq);
}


/**
 * Constructor to initialize the ReceiverLink with a socket and its send and receive addresses.
 * @param socket The UDP socket used for communication.
 * @param source_addr The address to which messages will be sent.
 * @param dest_addr The address from which messages will be received.
 */
ReceiverLink::ReceiverLink(int socket, sockaddr_in source_addr, sockaddr_in dest_addr): Link(socket, source_addr, dest_addr) {}

/**
 * Add message to delivered list, send ACK to sender, and return true if message was not already delivered. Otherwise, return false.
 * @param m_seq The sequence number of the message to be delivered.
 * @return True if the message was not already delivered, false otherwise.
 */
bool ReceiverLink::deliver(uint32_t m_seq)
{
  if (m_seq > last_delivered + 1) {
    throw std::runtime_error("Window with size >1 receive not implemented yet.");
  }

  if (m_seq == last_delivered + 1) {
    last_delivered = m_seq;

    // Transform message sequence number into big-endian for network transport
    std::vector<char> buffer = encodeMessage(m_seq, ACK);

    // Send ACK to sender
    if (sendto(socket, buffer.data(), Link::buffer_size, 0, reinterpret_cast<const sockaddr *>(&dest_addr), sizeof(dest_addr)) < 0) {
      std::ostringstream os;
      os << "Failed to send ACK " << m_seq << " from " << source_addr.sin_addr.s_addr << ":" << source_addr.sin_port << " to " << dest_addr.sin_addr.s_addr << ":" << dest_addr.sin_port;
      throw std::runtime_error(os.str());
    }

    return true;
  }
  return false;
}
