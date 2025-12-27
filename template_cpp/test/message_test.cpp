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
  std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET> seqs = {1, 2, 3, 4, 5, 6, 7, 8};
  std::array<Message, MAX_MESSAGES_PER_PACKET> msgs = {Message(10, origin_id),
                                                        Message(20, origin_id),
                                                        Message(30, origin_id),
                                                        Message(40, origin_id),
                                                        Message(50, origin_id),
                                                        Message(60, origin_id),
                                                        Message(70, origin_id),
                                                        Message(80, origin_id)};
  Packet msg(MES, nb_mes, seqs, msgs);
  msg.displayPacket();  

  const char* serialized = msg.serialize();
  Packet::displaySerialized(serialized);
  Packet deserialized_msg = Packet::deserialize(serialized);

  deserialized_msg.displayPacket();  

  IS_TRUE(deserialized_msg.getType() == MES);
  IS_TRUE(deserialized_msg.getNbMes() == nb_mes);
  IS_TRUE(deserialized_msg.getSeqs() == seqs);
  IS_TRUE(deserialized_msg.getMessages() == msgs);
}

int main() {
  testPacketSerialization();
  return test_failed ? 1 : 0;
}