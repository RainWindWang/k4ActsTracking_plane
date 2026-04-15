#include "TrackFittingAlg.h"
#include "TrackFittingTool.h"

DECLARE_COMPONENT(TrackFittingAlg)

TrackFittingAlg::TrackFittingAlg(const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode TrackFittingAlg::initialize() {
  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve TrackFittingTool" << endmsg;
    return StatusCode::FAILURE;
  }
  return StatusCode::SUCCESS;
}

StatusCode TrackFittingAlg::execute(const EventContext& ctx) const {
  const auto* inTracks = m_inTracks.get();
  const auto* inTrackStates = m_inTrackStates.get();

  if (!inTracks || !inTrackStates) {
    error() << "Missing input: "
            << "tracks=" << (inTracks ? "ok" : "null") << ", "
            << "trackStates=" << (inTrackStates ? "ok" : "null") << endmsg;
    return StatusCode::FAILURE;
  }

  auto outTracks = std::make_shared<k4ActsTracking::ActsTrackContainer>();
  auto outTrackStates = std::make_shared<k4ActsTracking::ActsTrackStateContainer>();

  if (m_tool->fitTracks(*inTracks, *inTrackStates, outTracks, outTrackStates).isFailure()) {
    error() << "Track fitting step failed" << endmsg;
    return StatusCode::FAILURE;
  }

  debug() << "TrackFittingAlg output: nTracks=" << outTracks->size()
          << ", nTrackStates=" << outTrackStates->size() << endmsg;

  m_outTracks.put(outTracks);
  m_outTrackStates.put(outTrackStates);
  return StatusCode::SUCCESS;
}
