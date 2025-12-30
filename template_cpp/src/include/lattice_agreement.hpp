#pragma once

#include <algorithm>
#include <stdint.h>
#include <set>

#include "globals.hpp"
#include "maps.hpp"
#include "message.hpp"

class Node; 

// Lattice agreement instance
class LatticeAgreementInstance {
public:
  LatticeAgreementInstance() = default;
  LatticeAgreementInstance(size_t nb_nodes, uint32_t ds, Node *p, prop_nb_t instance_id);
  ~LatticeAgreementInstance() = default;

  /**
   * process the message
   * @return True if the instance should be destroyed (has acknowledged proposals of all other nodes). False otherwise
   */
  bool processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port);
  void propose(std::set<proposal_t> proposal);
  void waitUntilDecidedOrTerminated();
  void terminate();

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

  /**
   * Update proposal using current proposal and accepted proposal
   */
  void updateProposal();

private:
  prop_nb_t instance_id;
  bool has_proposal = false;
  std::mutex la_mutex;
  bool terminated = false;

  // Proposer
  bool active = false;
  uint32_t ack_count = 0;
  uint32_t nack_count = 0;
  uint32_t active_proposal_number = 0;
  std::set<proposal_t> proposed_values;

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

// General lattice agreement class
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
  void propose(prop_nb_t instance_id, std::set<proposal_t> proposal);

  /**
   * Wait unitl lattice agreement instance has decided (block until decided)
   */
  void waitUntilDecidedOrTerminated(prop_nb_t instance_id);

  /**
   * Terminate this lattice agreement manager
   */
  void terminate();

private:
  /**
   * Try adding new instance to deque, if already exists do nothing
   */
  void tryAddingInstance(prop_nb_t instance_id);

private:
  std::map<prop_nb_t, std::unique_ptr<LatticeAgreementInstance>> instances;
  std::mutex la_manager_mutex;

  size_t nb_nodes;
  uint32_t distinct_values;
  Node *parent;
};