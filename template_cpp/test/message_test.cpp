#include "message.hpp"
#include <iostream>

// If parameter is not true, test fails
// This check function would be provided by the test framework
static int test_failed = 0;
#define IS_TRUE(x) do { if (!(x)) {std::cout << __FUNCTION__ << " failed on line " << __LINE__ << std::endl; test_failed = 1; } } while (0)

static void testPacketSerialization() {
  // std::cout << "Running " << __FUNCTION__ << std::endl;

  proc_id_t origin_id = 123456789;
  uint8_t nb_mes = 8;
  std::vector<std::pair<pkt_seq_t, Message>> payload = {{1, Message(10, origin_id)},
                                                        {2, Message(20, origin_id)},
                                                        {3, Message(30, origin_id)},
                                                        {4, Message(40, origin_id)},
                                                        {5, Message(50, origin_id)},
                                                        {6, Message(60, origin_id)},
                                                        {7, Message(70, origin_id)},
                                                        {8, Message(80, origin_id)} };
  Packet msg(MES, nb_mes, payload);
  Packet::displayPacket(msg);  

  const char* serialized = msg.serialize();
  Packet::displaySerialized(serialized);
  Packet deserialized_msg = Packet::deserialize(serialized);

  Packet::displayPacket(deserialized_msg);  

  IS_TRUE(deserialized_msg.getType() == MES);
  IS_TRUE(deserialized_msg.getNbMes() == nb_mes);
  IS_TRUE(deserialized_msg.getPayloads() == payload);
}

int main() {
  testPacketSerialization();
  return test_failed ? 1 : 0;
}