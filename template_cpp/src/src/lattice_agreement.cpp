#include "lattice_agreement.hpp"
#include "node.hpp"

LatticeAgreement::LatticeAgreement(size_t nb_nodes, uint32_t ds, Node *p)
  : nb_nodes(nb_nodes), distinct_values(ds), parent(p)
{}