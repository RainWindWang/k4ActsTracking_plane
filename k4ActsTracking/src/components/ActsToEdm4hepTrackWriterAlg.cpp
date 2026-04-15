#include "ActsToEdm4hepTrackWriterAlg.h"
#include "ActsToEdm4hepTrackWriterTool.h"

DECLARE_COMPONENT(ActsToEdm4hepTrackWriterAlg)

ActsToEdm4hepTrackWriterAlg::ActsToEdm4hepTrackWriterAlg(
    const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode ActsToEdm4hepTrackWriterAlg::initialize() {
  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve ActsToEdm4hepTrackWriterTool" << endmsg;
    return StatusCode::FAILURE;
  }
  return StatusCode::SUCCESS;
}

StatusCode ActsToEdm4hepTrackWriterAlg::execute(const EventContext& ctx) const {
  const auto* inTracks = m_inTracks.get();
  const auto* inTrackStates = m_inTrackStates.get();

  if (!inTracks || !inTrackStates) {
    error() << "Missing input: "
            << "tracks=" << (inTracks ? "ok" : "null") << ", "
            << "trackStates=" << (inTrackStates ? "ok" : "null") << endmsg;
    return StatusCode::FAILURE;
  }

  auto outTracks = std::make_unique<edm4hep::TrackCollection>();
  auto outTrackStates = std::make_unique<edm4hep::TrackStateCollection>();

  if (m_tool->write(*inTracks, *inTrackStates, *outTracks, *outTrackStates).isFailure()) {
    error() << "Failed to write ACTS tracks back to EDM4hep" << endmsg;
    return StatusCode::FAILURE;
  }

  debug() << "ActsToEdm4hepTrackWriterAlg output: nTracks=" << outTracks->size()
          << ", nTrackStates=" << outTrackStates->size() << endmsg;

  m_outTracks.put(std::move(outTracks));
  m_outTrackStates.put(std::move(outTrackStates));

  return StatusCode::SUCCESS;
}
