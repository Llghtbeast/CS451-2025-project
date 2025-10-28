#include "helper.hpp"

/**
 * Set up address from host
 */
sockaddr_in setupIpAddress(Parser::Host host)
{
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = host.port;
  addr.sin_addr.s_addr = host.ip;
  return addr;
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