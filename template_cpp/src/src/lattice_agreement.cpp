#include "lattice_agreement.hpp"
#include "node.hpp"

LatticeAgreement::LatticeAgreement(size_t nb_nodes, uint32_t ds, Node *p)
  : nb_nodes(nb_nodes), distinct_values(ds), parent(p)
{}

void LatticeAgreement::reset()
{
  active = false;
  ack_count = 0;
  nack_count = 0;
  active_proposal_number = 0;
}

void LatticeAgreement::processMessage(Message msg, std::string sender_ip_and_port)
{
}

void LatticeAgreement::propose(std::set<proposal_t> proposal)
{
  proposed_values = std::move(proposal);
  active = true;
  
  // Create message
  Message msg();

  parent->broadcast();
}

void LatticeAgreement::waitUntilDecided()
{
}

void LatticeAgreement::decide()
{
}
