#include "message.hpp"
#include <iostream>

// If parameter is not true, test fails
// This check function would be provided by the test framework
static int test_failed = 0;
#define IS_TRUE(x) do { if (!(x)) {std::cout << __FUNCTION__ << " failed on line " << __LINE__ << std::endl; test_failed = 1; } } while (0)

static void testMessageSerialization() {
  std::cout << "Running " << __FUNCTION__ << std::endl;

  proc_id_t origin_id = 123456789;
  uint8_t nb_mes = 8;
  std::vector<msg_seq_t> seqs = {2, 3, 4, 5, 6, 7, 8, 9};
  Message msg(MES, origin_id, nb_mes, seqs);

  const char* serialized = msg.serialize();
  Message::displaySerialized(serialized);
  Message deserialized_msg = Message::deserialize(serialized);
  

  IS_TRUE(deserialized_msg.getType() == MES);
  IS_TRUE(deserialized_msg.getOriginId() == origin_id);
  IS_TRUE(deserialized_msg.getNbMes() == nb_mes);
  IS_TRUE(deserialized_msg.getSeqs() == seqs);
}

int main() {
  testMessageSerialization();
  return test_failed ? 1 : 0;
}