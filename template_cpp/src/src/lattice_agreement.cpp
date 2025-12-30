#include "lattice_agreement.hpp"
#include "node.hpp"

// Single-shot Lattice agreement object
LatticeAgreementInstance::LatticeAgreementInstance(size_t nb_nodes, uint32_t ds, Node *p, prop_nb_t instance_id)
  : instance_id(instance_id), nb_nodes(nb_nodes), distinct_values(ds), parent(p)
{}

bool LatticeAgreementInstance::processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port)
{
  // Lock to avoid processing a message at the same time as resetting and proposing
  std::unique_lock<std::mutex> lock(la_mutex);
  
  switch (msg->type)
  {
  // Acceptor code
  case MessageType::MES:
    // std::cout << "msg proposal set: { ";
    // for (const auto& value: msg->proposed_values)
    // {
    //   std::cout << value << " ";
    // }
    // std::cout << "}\naccepted set: { ";
    // for (const auto& value: accepted_values)
    // {
    //   std::cout << value << " ";
    // }
    // std::cout << "}\n";


    if (std::includes(
      msg->proposed_values.begin(), msg->proposed_values.end(),   // Set proposed by other node
      accepted_values.begin(), accepted_values.end()))            // Local accepted set
    {
      accepted_values.insert(msg->proposed_values.begin(), msg->proposed_values.end());
      respond(msg, sender_ip_and_port, true);
      acknowledgements_sent++;
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
        // Reset attributes
        active_proposal_number++;
        ack_count = 0;
        nack_count = 0;

        // Update own proposal based on accepted_values (msg's proposal already added before)
        // Optimization to avoid sending set nacked by self
        updateProposal();
        // std::cout << "Proposal updated, broadcasting\n";
        broadcastProposal();

        // Check for majority ack (1 or 2 process case)
        if (ack_count >= nb_nodes/2 && active)
        {
          active = false;
          decide();
        }
      }
    }
    break;  

  default:
    break;  
  }

  // If instance has decided and acknowledged proposals from all other nodes, return true
  return decided && acknowledgements_sent == nb_nodes;
}

void LatticeAgreementInstance::propose(std::set<proposal_t> proposal)
{
  // Lock to avoid proposing at the same time as processing a message
  std::lock_guard<std::mutex> lock(la_mutex);

  has_proposal = true;
  active = true;
  
  // Merge proposal with accepted_values to accept or reject its own proposal
  proposed_values = std::move(proposal);
  updateProposal();
  
  // TODO: check if rebroadcast is  necessary (proposal contained in accepted set)
  broadcastProposal();
}

void LatticeAgreementInstance::waitUntilDecidedOrTerminated()
{
  std::unique_lock<std::mutex> lock(decision_mutex);
  decision_cv.wait(lock, [this]{ return decided || terminated; });
  // std::cout << "LatticeAgreementInstance " << instance_id << " exited wait\n";
}

void LatticeAgreementInstance::terminate()
{
  // std::cout << "LatticeAgreementInstance " << instance_id << " terminated\n";
  terminated = true;
  decision_cv.notify_all();
}

// Private methods:
void LatticeAgreementInstance::broadcastProposal()
{
  // Create message
  auto msg_ptr = std::make_shared<Message>(MessageType::MES, instance_id, active_proposal_number, proposed_values);
  parent->broadcast(msg_ptr);
}

void LatticeAgreementInstance::respond(std::shared_ptr<const Message> msg, std::string sender_ip_and_port, bool acknowledge)
{
  // Create response
  auto response = acknowledge ? msg->toAck() : msg->toNack(accepted_values);
  parent->sendTo(std::make_shared<Message>(std::move(response)), sender_ip_and_port);
}

void LatticeAgreementInstance::decide()
{
  std::lock_guard<std::mutex> lock(decision_mutex);
  if (decided) return;
  if (!has_proposal) return;

  // std::cout << "Decided set { ";
  // for (const auto& value: proposed_values)
  // {
  //   std::cout << value << " ";
  // }
  // std::cout << "}\n\n";

  decided = true;
  active = false;
  parent->logger->logDecision(proposed_values);

  decision_cv.notify_one();
}

void LatticeAgreementInstance::updateProposal()
{
  proposed_values.insert(accepted_values.begin(), accepted_values.end());

  // Accept own proposal
  accepted_values = proposed_values;
  acknowledgements_sent = 1;
  ack_count = 1;
}

// Multi-shot Lattice agreement object
LatticeAgreement::LatticeAgreement(size_t nb_nodes, uint32_t ds, Node *p)
  : nb_nodes(nb_nodes), distinct_values(ds), parent(p)
{}

void LatticeAgreement::processMessage(std::shared_ptr<const Message> msg, std::string sender_ip_and_port)
{
  std::lock_guard<std::mutex> lock(la_manager_mutex);
  
  // std::cout << "processing message from " << sender_ip_and_port << ": ";
  // msg.get()->displayMessage();

  tryAddingInstance(msg->instance);

  // Process message and destroy instance if not needed anymore
  bool destroy_instance = instances[msg->instance]->processMessage(msg, sender_ip_and_port);
  if (destroy_instance)
  {
    std::cout << "Destroying LA instance " << msg->instance << ", all processes have decided for that instance\n";
    instances.erase(msg->instance);
  }
}

void LatticeAgreement::propose(prop_nb_t instance_id, std::set<proposal_t> proposal)
{
  std::lock_guard<std::mutex> lock(la_manager_mutex);

  // std::cout << "Proposing (instance " << instance_id << "): { ";
  // for (const auto& value: proposal)
  // {
  //   std::cout << value << " ";
  // }
  // std::cout << "}\n";

  tryAddingInstance(instance_id);
  // add proposal
  instances[instance_id]->propose(std::move(proposal));
}

void LatticeAgreement::waitUntilDecidedOrTerminated(prop_nb_t instance_id)
{
  instances[instance_id]->waitUntilDecidedOrTerminated();
}

void LatticeAgreement::terminate()
{
  for (const auto& instance: instances)
  {
    instance.second->terminate();
  }
}

// Private methods:
void LatticeAgreement::tryAddingInstance(prop_nb_t instance_id)
{
  // Create instance if it does not exist
  if (instances.find(instance_id) == instances.end())
  {
    instances.emplace(instance_id, std::make_unique<LatticeAgreementInstance>(
      nb_nodes, distinct_values, parent, instance_id
    ));
  }
}
