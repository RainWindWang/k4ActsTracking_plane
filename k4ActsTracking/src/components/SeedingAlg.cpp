#include "SeedingAlg.h"

#include <fmt/format.h>

DECLARE_COMPONENT(SeedingAlg)

SeedingAlg::SeedingAlg(const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode SeedingAlg::initialize() {
  K4_GAUDI_CHECK(Gaudi::Algorithm::initialize());
  K4_GAUDI_CHECK(m_seedingTool.retrieve());
  return StatusCode::SUCCESS;
}

StatusCode SeedingAlg::execute(const EventContext& /*ctx*/) const {
  const auto* in = m_inSpacePoints.get();
  if (!in) {
    error() << "SeedingAlg: input SpacePoints wrapper is null" << endmsg;
    return StatusCode::FAILURE;
  }

  auto* outSeeds = m_outSeeds.createAndPut();
  if (!outSeeds) {
    error() << "SeedingAlg: failed to create output seed container" << endmsg;
    return StatusCode::FAILURE;
  }

  StatusCode sc = m_seedingTool->createSeeds(in->sps, *outSeeds);
  if (!sc.isSuccess()) {
    error() << "SeedingAlg: SeedingTool failed" << endmsg;
    return sc;
  }

  if (m_verbose) {
    info() << fmt::format("SeedingAlg: spacePoints={}, seeds={}",
                          in->sps.size(), outSeeds->size())
           << endmsg;
  }

  return StatusCode::SUCCESS;
}
