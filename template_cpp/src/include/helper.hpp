#pragma once

#include <netinet/in.h>
#include <string>

#include "parser.hpp"

sockaddr_in setupIpAddress(Parser::Host host);
std::string ipAddressToString(sockaddr_in addr);
