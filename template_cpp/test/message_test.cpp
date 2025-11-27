#include "message.hpp"
#include <iostream>
#include <iomanip>

// If parameter is not true, test fails
// This check function would be provided by the test framework
static int test_failed = 0;
#define IS_TRUE(x) do { if (!(x)) {std::cout << __FUNCTION__ << " failed on line " << __LINE__ << std::endl; test_failed = 1; } } while (0)

static void testMessageSerialization() {
  std::cout << "Running " << __FUNCTION__ << std::endl;

  unsigned long origin_id = 123456789;
  uint8_t nb_mes = 8;
  std::vector<uint32_t> seqs = {2, 3, 4, 5, 6, 7, 8, 9};
  Message msg(MES, origin_id, nb_mes, seqs);

  const char* serialized = msg.serialize();
  size_t len = msg.serializedSize(); // implement this if it doesn't exist
  std::cout << "Serialized message size: " << len << std::endl;
  std::cout << "Serialized message (hex): ";
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(serialized[i]);
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << ' ';
  }
  std::cout << std::dec << std::setfill(' '); // reset formatting
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