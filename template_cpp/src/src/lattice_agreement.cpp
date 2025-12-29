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

  decided = false;
  accepted_values = {};
}

void LatticeAgreement::processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port)
{
  switch (msg->type)
  {
  // Acceptor code
  case MessageType::MES:
    if (std::includes(
      msg->proposed_values.begin(), msg->proposed_values.end(),   // Set proposed by other node
      accepted_values.begin(), accepted_values.end()))            // Local accepted set
    {
      accepted_values.insert(msg->proposed_values.begin(), msg->proposed_values.end());
      respond(msg, sender_ip_and_port, true);
    }
    else
    {
      accepted_values.insert(msg->proposed_values.begin(), msg->proposed_values.end());
      respond(msg, sender_ip_and_port, false);
    }
    break;

  // Proposer code
  case MessageType::ACK:
    if (msg->round == active_proposal_number)
    {
      ack_count++;

      // Check for majority ack
      if (ack_count >= nb_nodes/2 && active)
      {
        active = false;
        decide();
      }
    }
    break;  

  case MessageType::NACK:
    if (msg->round == active_proposal_number)
    {
      nack_count++;
      proposed_values.insert(msg->proposed_values.begin(), msg->proposed_values.end());

      // Check for majority response
      if (nack_count > 0 && (ack_count + nack_count) >= nb_nodes/2 && active)
      {
        active_proposal_number++;
        ack_count = 0;
        nack_count = 0;
        
        broadcastProposal();
      }
    }
    break;  
  default:
    break;  
  }
}

void LatticeAgreement::propose(std::set<proposal_t> proposal)
{
  proposed_values = proposal;
  accepted_values = std::move(proposal);
  ack_count++;
  active = true;
  
  broadcastProposal();
}

void LatticeAgreement::waitUntilDecided()
{
  std::unique_lock<std::mutex> lock(decision_mutex);
  decision_cv.wait(lock, [this]() { return decided; });
}

void LatticeAgreement::broadcastProposal()
{
  // Create message
  auto msg_ptr = std::make_shared<Message>(MessageType::MES, active_proposal_number, proposed_values);
  parent->broadcast(msg_ptr);
}

void LatticeAgreement::respond(std::shared_ptr<const Message> msg, std::string sender_ip_and_port, bool acknowledge)
{
  // Create response
  auto response = acknowledge ? msg->toAck() : msg->toNack(accepted_values);
  parent->sendTo(std::make_shared<Message>(std::move(response)), sender_ip_and_port);
}

void LatticeAgreement::decide()
{
  std::lock_guard<std::mutex> lock(decision_mutex);
  if (decided) return;

  std::cout << "Decided set { ";
  for (const auto& value: proposed_values)
  {
    std::cout << value << " ";
  }
  std::cout << "}\n";

  decided = true;
  active = false;
  parent->logger->logDecision(proposed_values);

  decision_cv.notify_one();
}
