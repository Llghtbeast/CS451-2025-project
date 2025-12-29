#pragma once

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
   * Reset object for a new round of lattice agreement, maybe not necessary when using multiple slots
   */
  void reset();

  /**
   * Process message from other node
   */
  void processMessage(Message msg, std::string sender_ip_and_port);

  /**
   * Propose a proposal
   */
  void propose(std::set<proposal_t> proposal);

  /**
   * Wait unitl lattice agreement instance has decided (block until decided)
   */
  void waitUntilDecided();

private:
  /**
   * Decide on a set of values (unblocks waitUntilDecided)
   */
  void decide();

private:
  // Proposer
  bool active = false;
  uint32_t ack_count = 0;
  uint32_t nack_count = 0;
  uint32_t active_proposal_number = 0;
  std::set<proposal_t> proposed_values;

  ConcurrentMap<uint32_t, std::set<proc_id_t>> acks_received;
  ConcurrentMap<uint32_t, std::set<proc_id_t>> nacks_received;

  // Acceptor
  std::set<proposal_t> accepted_values;

  size_t nb_nodes;
  uint32_t distinct_values;
  Node *parent;
};