#pragma once

#include <algorithm>
#include <stdint.h>
#include <set>

#include "globals.hpp"
#include "maps.hpp"
#include "message.hpp"

class Node;

class LatticeAgreement {
public:
  LatticeAgreement() = default;
  LatticeAgreement(size_t nb_nodes, uint32_t ds, Node *p);

  /**
   * Process message from other node
   */
  void processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port);

  /**
   * Propose a proposal
   */
  void propose(prop_nb_t instance, std::set<proposal_t> proposal);

  /**
   * Wait unitl lattice agreement instance has decided (block until decided)
   */
  void waitUntilDecided();

private:
  /**
   * Create message and broadcast
   */
  void broadcastProposal();

  /**
   * Respond to sender with ACK or NACK
   */
  void respond(std::shared_ptr<const Message> msg, std::string sender_ip_and_port, bool acknoledge);

  /**
   * Decide on a set of values (unblocks waitUntilDecided)
   */
  void decide();

private:
  prop_nb_t active_instance_number = 0;

  // Proposer
  bool active = false;
  uint32_t ack_count = 0;
  uint32_t nack_count = 0;
  uint32_t active_proposal_number = 0;
  std::set<proposal_t> proposed_values;

  std::mutex la_mutex;

  bool decided = false;
  std::mutex decision_mutex;
  std::condition_variable decision_cv;

  // Acceptor
  std::set<proposal_t> accepted_values;
  size_t acknowledgements_sent;

  size_t nb_nodes;
  uint32_t distinct_values;
  Node *parent;
};