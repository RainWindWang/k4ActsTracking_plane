#include "ActsToEdm4hepTrackWriterTool.h"

#include "k4ActsTracking/ActsDataContainers.h"

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <edm4hep/MutableTrack.h>
#include <edm4hep/TrackCollection.h>

#include <memory>

DECLARE_COMPONENT(ActsToEdm4hepTrackWriterTool)

namespace {

using TrackStateBackend = k4ActsTracking::ActsTrackStateContainer;
using TrackContainerBackend = k4ActsTracking::ActsTrackContainer;
using MutableTrackContainer =
    Acts::TrackContainer<TrackContainerBackend, TrackStateBackend,
                         std::shared_ptr>;

}  // namespace

ActsToEdm4hepTrackWriterTool::ActsToEdm4hepTrackWriterTool(
    const std::string& type, const std::string& name, const IInterface* parent)
    : extends<AlgTool, IActsToEdm4hepTrackWriterTool>(type, name, parent) {}

StatusCode ActsToEdm4hepTrackWriterTool::initialize() {
  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  info() << "Initialized ActsToEdm4hepTrackWriterTool"
         << ", DumpSummary=" << (m_dumpSummary ? "true" : "false")
         << endmsg;

  return StatusCode::SUCCESS;
}

StatusCode ActsToEdm4hepTrackWriterTool::writeTracks(
    const k4ActsTracking::ActsTrackContainerPtr& tracks,
    const k4ActsTracking::ActsTrackStateContainerPtr& trackStates,
    edm4hep::TrackCollection& outTracks) const {
  if (!tracks) {
    error() << "Input ACTS track container pointer is null" << endmsg;
    return StatusCode::FAILURE;
  }
  if (!trackStates) {
    error() << "Input ACTS track-state container pointer is null" << endmsg;
    return StatusCode::FAILURE;
  }

  MutableTrackContainer actsTracks(tracks, trackStates);

  std::size_t nActsTracks = 0;
  std::size_t nEdmTracks = 0;

  for (auto actsTrack : actsTracks) {
    ++nActsTracks;

    auto edmTrack = outTracks.create();

    // Conservative baseline output:
    // one ACTS track -> one edm4hep::Track
    // For now we do not force-fill detailed helix parameters / states,
    // because ACTS proxy API varies across versions.
    edmTrack.setType(0);
    edmTrack.setChi2(0.f);
    edmTrack.setNdf(0);
    //edmTrack.setRadiusOfInnermostHit(0.f);

    if (m_dumpSummary) {
      debug() << "Writer: converted ACTS track[" << (nActsTracks - 1) << "]"
              << " with nMeasurements=" << actsTrack.nMeasurements()
              << ", nStates=" << actsTrack.nTrackStates()
              << ", nHoles=" << actsTrack.nHoles()
              << ", nOutliers=" << actsTrack.nOutliers()
              << endmsg;
    }

    ++nEdmTracks;
  }

  if (m_dumpSummary) {
    info() << "ActsToEdm4hepTrackWriterTool wrote " << nEdmTracks
           << " edm4hep track(s) from " << nActsTracks << " ACTS track(s)"
           << endmsg;
  }

  return StatusCode::SUCCESS;
}
