#pragma once

#include <netinet/in.h>
#include <string>

#include "parser.hpp"

sockaddr_in setupIpAddress(Parser::Host host);
std::string ipAddressToString(sockaddr_in addr);

// Helper functions for 64-bit network byte order conversion
inline uint64_t htonll(uint64_t value) {
  static const int num = 42;
  // Check if system is little-endian using reinterpret_cast and const_cast
  if (*reinterpret_cast<const char*>(&num) == 42) { // Little-endian
    // Split 64-bit value into two 32-bit parts, convert each, then recombine
    uint32_t low = static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    uint32_t high = static_cast<uint32_t>(value >> 32);
    return (static_cast<uint64_t>(htonl(low)) << 32) | static_cast<uint64_t>(htonl(high));
  }
  return value; // Already big-endian
}

inline uint64_t ntohll(uint64_t value) {
  return htonll(value); // Conversion is symmetric
}
