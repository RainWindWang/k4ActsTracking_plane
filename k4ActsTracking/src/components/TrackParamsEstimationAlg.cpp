#include "TrackParamsEstimationAlg.h"

DECLARE_COMPONENT(TrackParamsEstimationAlg)

TrackParamsEstimationAlg::TrackParamsEstimationAlg(const std::string& name,
                                                   ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode TrackParamsEstimationAlg::initialize() {
  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve TrackParamsEstimationTool" << endmsg;
    return StatusCode::FAILURE;
  }

  m_inSP = k4FWCore::DataHandle<k4ActsTracking::SpacePointCollection>(
      m_inSpacePointsName.value(), Gaudi::DataHandle::Reader, this);

  m_inSeeds = k4FWCore::DataHandle<k4ActsTracking::SeedContainer>(
      m_inSeedsName.value(), Gaudi::DataHandle::Reader, this);

  m_inMeas = k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection>(
      m_inMeasurementsName.value(), Gaudi::DataHandle::Reader, this);

  m_outParams =
      k4FWCore::DataHandle<k4ActsTracking::InitialTrackParametersCollection>(
          m_outInitialTrackParametersName.value(), Gaudi::DataHandle::Writer, this);

  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve tool " << m_tool.typeAndName() << endmsg;
    return StatusCode::FAILURE;
  }  

  return StatusCode::SUCCESS;
}

StatusCode TrackParamsEstimationAlg::execute(const EventContext& ctx) const {
  const auto* spWrap = m_inSP.get();
  const auto* seeds  = m_inSeeds.get();
  const auto* meas   = m_inMeas.get();

  if (!spWrap || !seeds || !meas) {
    error() << "Missing input: "
            << "spacePoints=" << (spWrap ? "ok" : "null") << ", "
            << "seeds=" << (seeds ? "ok" : "null") << ", "
            << "measurements=" << (meas ? "ok" : "null") << endmsg;
    return StatusCode::FAILURE;
  }

  auto out = std::make_unique<k4ActsTracking::InitialTrackParametersCollection>();
  out->reserve(seeds->size());

  for (std::size_t i = 0; i < seeds->size(); ++i) {
    const auto seed = (*seeds)[i];
    auto initPars = m_tool->estimateOneSeed(spWrap->sps, seed, *meas);
    if (initPars.has_value()) {
      out->push_back(std::move(*initPars));
    }
  }

  debug() << "TrackParamsEstimationAlg: produced " << out->size()
          << " initial parameter set(s) from " << seeds->size()
          << " seed(s)" << endmsg;

  m_outParams.put(std::move(out));
  return StatusCode::SUCCESS;
}
