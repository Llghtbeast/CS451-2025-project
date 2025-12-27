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
  std::vector<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>> payload = {{1, origin_id, 10},
                                                                      {2, origin_id, 20},
                                                                      {3, origin_id, 30},
                                                                      {4, origin_id, 40},
                                                                      {5, origin_id, 50},
                                                                      {6, origin_id, 60},
                                                                      {7, origin_id, 70},
                                                                      {8, origin_id, 80} };

  Packet msg(MES, nb_mes, payload);

  const char* serialized = msg.serialize();
  Packet::displaySerialized(serialized);
  Packet deserialized_msg = Packet::deserialize(serialized);
  

  IS_TRUE(deserialized_msg.getType() == MES);
  IS_TRUE(deserialized_msg.getNbMes() == nb_mes);
  IS_TRUE(deserialized_msg.getPayloads() == payload);
}

int main() {
  testPacketSerialization();
  return test_failed ? 1 : 0;
}