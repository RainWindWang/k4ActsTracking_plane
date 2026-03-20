#include "TrackerEDMToActsAlg.h"

#include <k4FWCore/GaudiChecks.h>

#include <fmt/format.h>

DECLARE_COMPONENT(TrackerEDMToActsAlg)

TrackerEDMToActsAlg::TrackerEDMToActsAlg(const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode TrackerEDMToActsAlg::initialize() {
  K4_GAUDI_CHECK(Gaudi::Algorithm::initialize());
  K4_GAUDI_CHECK(m_tool.retrieve());
  return StatusCode::SUCCESS;
}

StatusCode TrackerEDMToActsAlg::execute(const EventContext& ctx) const {
  const auto* in = m_inHits.get();
  if (!in) {
    error() << "Input TrackerHitPlaneCollection is null" << endmsg;
    return StatusCode::FAILURE;
  }

  auto* sp = m_outSP.createAndPut();
  auto* sl = m_outSL.createAndPut();
  auto* mp = m_outMeas.createAndPut();

  if (!sp || !sl || !mp) {
    error() << "Failed to create one or more output collections" << endmsg;
    return StatusCode::FAILURE;
  }

  StatusCode sc = m_tool->convert(ctx, *in, *sp, *sl, *mp);
  if (!sc.isSuccess()) {
    error() << "Converter tool failed" << endmsg;
    return sc;
  }

debug() << fmt::format(
               "TrackerEDMToActsAlg: inHits={}, outSP={}, outSL={}, outMeas={}",
               in->size(), sp->sps.size(), sl->size(), mp->measurements().size())
        << endmsg;

  return StatusCode::SUCCESS;
}
