#include "TrackFindingCKFAlg.h"
#include "TrackFindingCKFTool.h"

DECLARE_COMPONENT(TrackFindingCKFAlg)

TrackFindingCKFAlg::TrackFindingCKFAlg(const std::string& name,
                                       ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode TrackFindingCKFAlg::initialize() {
  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve TrackFindingCKFTool" << endmsg;
    return StatusCode::FAILURE;
  }
  return StatusCode::SUCCESS;
}

StatusCode TrackFindingCKFAlg::execute(const EventContext& ctx) const {
  const auto* params = m_inParams.get();
  const auto* meas   = m_inMeasurements.get();

  if (!params || !meas) {
    error() << "Missing input: "
            << "params=" << (params ? "ok" : "null") << ", "
            << "measurements=" << (meas ? "ok" : "null") << endmsg;
    return StatusCode::FAILURE;
  }

  auto tracks = std::make_shared<k4ActsTracking::ActsTrackContainer>();
  auto trackStates = std::make_shared<k4ActsTracking::ActsTrackStateContainer>();

  debug() << "TrackFindingCKFAlg input: nInitialParams=" << params->size()
          << ", nMeasurements=" << meas->size() << endmsg;

  std::size_t nFailed = 0;
  for (const auto& p : *params) {
    if (m_tool->findTracks(p, *meas, tracks, trackStates).isFailure()) {
      ++nFailed;
      warning() << "CKF failed for one initial parameter" << endmsg;
    }
  }

  debug() << "TrackFindingCKFAlg output: nTracks=" << tracks->size()
          << ", nTrackStates=" << trackStates->size()
          << ", nFailedSeeds=" << nFailed << endmsg;

  m_outTracks.put(tracks);
  m_outTrackStates.put(trackStates);

  return StatusCode::SUCCESS;
}
