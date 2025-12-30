#include "lattice_agreement.hpp"
#include "node.hpp"

LatticeAgreement::LatticeAgreement(size_t nb_nodes, uint32_t ds, Node *p)
  : nb_nodes(nb_nodes), distinct_values(ds), parent(p)
{}

void LatticeAgreement::processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port)
{
  // Lock to avoid processing a message at the same time as resetting and proposing
  std::lock_guard<std::mutex> lock(la_mutex);
  
  std::cout << "processing message: ";
  msg.get()->displayMessage();

  switch (msg->type)
  {
  // Acceptor code
  case MessageType::MES:
    // Ensure accepted_values is properly sized for this instance
    if (active_instance_number < msg->instance)
    
    if (std::includes(
      msg->proposed_values.begin(), msg->proposed_values.end(),                             // Set proposed by other node
      accepted_values[msg->instance-1].begin(), accepted_values[msg->instance-1].end()))    // Local accepted set
    {
      accepted_values[msg->instance-1].insert(msg->proposed_values.begin(), msg->proposed_values.end());
      respond(msg, sender_ip_and_port, true);
    }
    else
    {
      accepted_values[msg->instance-1].insert(msg->proposed_values.begin(), msg->proposed_values.end());
      respond(msg, sender_ip_and_port, false);
    }
    return;

  // Proposer code
  case MessageType::ACK:
    // If the message corresponds to a different lattice agreement instance, skip
    if (active_instance_number != msg->instance) return;

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
    return;  

  case MessageType::NACK:
    // If the message corresponds to a different lattice agreement instance, skip
    if (active_instance_number != msg->instance) return;

    if (msg->round == active_proposal_number)
    {
      nack_count++;
      proposed_values.insert(msg->proposed_values.begin(), msg->proposed_values.end());

      // Check for majority response
      if (nack_count > 0 && (ack_count + nack_count) >= nb_nodes/2 && active)
      {
        // Reset attributes
        active_proposal_number++;
        ack_count = 0;
        nack_count = 0;

        // Update own proposal based on accepted_values
        proposed_values.insert(accepted_values[msg->instance-1].begin(), accepted_values[msg->instance-1].end());
        accepted_values[msg->instance-1] = proposed_values;
        ack_count++;
        
        broadcastProposal();
      }
    }
    return;  

  default:
    return;  
  }
}

void LatticeAgreement::propose(prop_nb_t instance, std::set<proposal_t> proposal)
{
  // Lock to ensure a correct reset + proposal
  std::lock_guard<std::mutex> lock(la_mutex);

  std::cout << "Proposing (instance " << instance << "): { ";
  for (const auto& value: proposal)
  {
    std::cout << value << " ";
  }
  std::cout << "}\n";

  // Reset all attributes
  active_instance_number = instance;
  ack_count = 0;
  nack_count = 0;
  active_proposal_number = 0;
  proposed_values = proposal;
  decided = false;
  active = true;
  
  // Resize deque if needed
  if (accepted_values.size() + first_instance_id <= instance)
  {
    accepted_values.resize(instance - first_instance_id + 1);
  }
  // Accept own proposal
  accepted_values[instance-1] = std::move(proposal);
  ack_count++;
  
  broadcastProposal();
}

void LatticeAgreement::waitUntilDecided()
{
  std::unique_lock<std::mutex> lock(decision_mutex);
  decision_cv.wait(lock, [this]() { return decided; });
}

// Private methods:
void LatticeAgreement::broadcastProposal()
{
  // Create message
  auto msg_ptr = std::make_shared<Message>(MessageType::MES, active_instance_number, active_proposal_number, proposed_values);
  parent->broadcast(msg_ptr);
}

void LatticeAgreement::respond(std::shared_ptr<const Message> msg, std::string sender_ip_and_port, bool acknowledge)
{
  // Create response
  auto response = acknowledge ? msg->toAck() : msg->toNack(accepted_values[msg->instance-1]);
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
  std::cout << "}\n\n";

  decided = true;
  active = false;
  parent->logger->logDecision(proposed_values);

  decision_cv.notify_one();
}
