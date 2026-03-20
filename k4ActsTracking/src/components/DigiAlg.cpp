#include "DigiAlg.h"

#include <k4FWCore/GaudiChecks.h>

DECLARE_COMPONENT(DigiAlg)

DigiAlg::DigiAlg(const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode DigiAlg::initialize() {
  K4_GAUDI_CHECK(Gaudi::Algorithm::initialize());
  K4_GAUDI_CHECK(m_tools.retrieve());
  return StatusCode::SUCCESS;
}

StatusCode DigiAlg::execute(const EventContext& ctx) const {
  const auto* in = m_inHits.get();
  if (!in) {
    error() << "Failed to read input collection: " << m_inputName.value() << endmsg;
    return StatusCode::FAILURE;
  }

  auto* out = m_outHits.createAndPut();
  if (!out) {
    error() << "Failed to create output collection: " << m_outputName.value() << endmsg;
    return StatusCode::FAILURE;
  }

  for (const auto& tool : m_tools) {
    K4_GAUDI_CHECK(tool->digitize(ctx, *in, *out));
  }

  return StatusCode::SUCCESS;
}
