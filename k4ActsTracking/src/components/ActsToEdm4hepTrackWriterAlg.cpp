#include "ActsToEdm4hepTrackWriterAlg.h"

#include <GaudiKernel/ISvcLocator.h>
#include <GaudiKernel/MsgStream.h>

DECLARE_COMPONENT(ActsToEdm4hepTrackWriterAlg)

ActsToEdm4hepTrackWriterAlg::ActsToEdm4hepTrackWriterAlg(
    const std::string& name, ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode ActsToEdm4hepTrackWriterAlg::initialize() {
  if (Gaudi::Algorithm::initialize().isFailure()) {
    return StatusCode::FAILURE;
  }

  m_inTracks =
      k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr>(
          m_inTracksName.value(), Gaudi::DataHandle::Reader, this);

  m_inTrackStates =
      k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr>(
          m_inTrackStatesName.value(), Gaudi::DataHandle::Reader, this);

  m_outTracks =
      k4FWCore::DataHandle<edm4hep::TrackCollection>(
          m_outTracksName.value(), Gaudi::DataHandle::Writer, this);

  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve ACTS->EDM4hep writer tool "
            << m_tool.typeAndName() << endmsg;
    return StatusCode::FAILURE;
  }

  return StatusCode::SUCCESS;
}

StatusCode ActsToEdm4hepTrackWriterAlg::execute(
    const EventContext& /*ctx*/) const {
  const auto* inTracks = m_inTracks.get();
  const auto* inTrackStates = m_inTrackStates.get();

  if (inTracks == nullptr) {
    error() << "Input ActsTracks is missing" << endmsg;
    return StatusCode::FAILURE;
  }
  if (inTrackStates == nullptr) {
    error() << "Input ActsTrackStates is missing" << endmsg;
    return StatusCode::FAILURE;
  }

  auto outTracks = std::make_unique<edm4hep::TrackCollection>();

  if (m_tool->writeTracks(*inTracks, *inTrackStates, *outTracks).isFailure()) {
    error() << "ACTS->EDM4hep writer tool failed" << endmsg;
    return StatusCode::FAILURE;
  }

  debug() << "ActsToEdm4hepTrackWriterAlg produced "
          << outTracks->size() << " edm4hep track(s)" << endmsg;

  m_outTracks.put(std::move(outTracks));

  return StatusCode::SUCCESS;
}
