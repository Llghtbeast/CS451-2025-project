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
  std::array<std::shared_ptr<const Message>, MAX_MESSAGES_PER_PACKET> msgs = {
    std::make_shared<Message>(MessageType::MES, 10, 1, std::set<proposal_t>({ 1, 2, 3 })),
    std::make_shared<Message>(MessageType::MES, 10, 2, std::set<proposal_t>({ 1, 2 })),
    std::make_shared<Message>(MessageType::ACK, 10, 3, std::set<proposal_t>({})),
    std::make_shared<Message>(MessageType::NACK, 10, 4, std::set<proposal_t>({ 2, 4, 5 })),
    std::make_shared<Message>(MessageType::MES, 10, 5, std::set<proposal_t>({ 1, 2, 3, 4, 5})),
    std::make_shared<Message>(MessageType::NACK, 10, 6, std::set<proposal_t>({ 2})),
    std::make_shared<Message>(MessageType::ACK, 10, 7, std::set<proposal_t>({})),
    std::make_shared<Message>(MessageType::ACK, 10, 8, std::set<proposal_t>({}))
  };
  Packet msg(MES, nb_mes, seqs, msgs);
  msg.displayPacket();  

  const char* serialized = msg.serialize();
  Packet::displaySerialized(serialized);
  Packet deserialized_pkt = Packet::deserialize(serialized);

  deserialized_pkt.displayPacket();  

  IS_TRUE(deserialized_pkt.getType() == MES);
  IS_TRUE(deserialized_pkt.getNbMes() == nb_mes);
  IS_TRUE(deserialized_pkt.getSeqs() == seqs);

  auto deserialized_msgs = deserialized_pkt.getMessages();
  for (size_t i = 0; i < nb_mes; i++)
  {
    IS_TRUE(*deserialized_msgs[i] == *msgs[i]);
  }
}

int main() {
  testPacketSerialization();
  return test_failed ? 1 : 0;
}