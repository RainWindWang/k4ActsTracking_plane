#include "TrackFindingCKFAlg.h"

#include <GaudiKernel/ISvcLocator.h>
#include <GaudiKernel/MsgStream.h>

DECLARE_COMPONENT(TrackFindingCKFAlg)

TrackFindingCKFAlg::TrackFindingCKFAlg(const std::string& name,
                                       ISvcLocator* svcLoc)
    : Gaudi::Algorithm(name, svcLoc) {}

StatusCode TrackFindingCKFAlg::initialize() {
  if (Gaudi::Algorithm::initialize().isFailure()) {
    return StatusCode::FAILURE;
  }

  m_inInitialTrackParameters =
      k4FWCore::DataHandle<k4ActsTracking::InitialTrackParametersCollection>(
          m_inInitialTrackParametersName.value(), Gaudi::DataHandle::Reader, this);

  m_inSourceLinks =
      k4FWCore::DataHandle<k4ActsTracking::SourceLinkCollection>(
          m_inSourceLinksName.value(), Gaudi::DataHandle::Reader, this);

  m_inMeasurements =
      k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection>(
          m_inMeasurementsName.value(), Gaudi::DataHandle::Reader, this);

  m_outTracks =
      k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr>(
          m_outTracksName.value(), Gaudi::DataHandle::Writer, this);

  m_outTrackStates =
      k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr>(
          m_outTrackStatesName.value(), Gaudi::DataHandle::Writer, this);

  if (m_tool.retrieve().isFailure()) {
    error() << "Failed to retrieve track finding tool " 
	    << m_tool.typeAndName() << endmsg;
    return StatusCode::FAILURE;
  }

  return StatusCode::SUCCESS;
}

StatusCode TrackFindingCKFAlg::execute(const EventContext& /*ctx*/) const {
  const auto* initialParameters = m_inInitialTrackParameters.get();
  const auto* sourceLinks = m_inSourceLinks.get();
  const auto* measurements = m_inMeasurements.get();

  if (initialParameters == nullptr) {
    error() << "Input TrackerInitialTrackParameters is missing" << endmsg;
    return StatusCode::FAILURE;
  }
  if (sourceLinks == nullptr) {
    error() << "Input TrackerSourceLinks is missing" << endmsg;
    return StatusCode::FAILURE;
  }
  if (measurements == nullptr) {
    error() << "Input TrackerMeasurements is missing" << endmsg;
    return StatusCode::FAILURE;
  }

  auto outTracks = std::make_shared<k4ActsTracking::ActsTrackContainer>();
  auto outTrackStates =
      std::make_shared<k4ActsTracking::ActsTrackStateContainer>();

  if (m_tool
          ->findTracks(*initialParameters, *sourceLinks, *measurements,
                       outTracks, outTrackStates)
          .isFailure()) {
    error() << "Track finding tool failed" << endmsg;
    return StatusCode::FAILURE;
  }

  auto outTracksObj =
      std::make_unique<k4ActsTracking::ActsTrackContainerPtr>(outTracks);
  auto outTrackStatesObj =
      std::make_unique<k4ActsTracking::ActsTrackStateContainerPtr>(outTrackStates);

  m_outTracks.put(std::move(outTracksObj));
  m_outTrackStates.put(std::move(outTrackStatesObj));

  return StatusCode::SUCCESS;
}
