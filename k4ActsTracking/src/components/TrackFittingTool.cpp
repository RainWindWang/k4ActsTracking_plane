#include "TrackFittingTool.h"

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Utilities/Logger.hpp>

DECLARE_COMPONENT(TrackFittingTool)

namespace {
using MutableTrackContainer =
    Acts::TrackContainer<Acts::VectorTrackContainer,
                         Acts::VectorMultiTrajectory,
                         std::shared_ptr>;
}

TrackFittingTool::TrackFittingTool(const std::string& type,
                                   const std::string& name,
                                   const IInterface* parent)
    : extends(type, name, parent) {}

StatusCode TrackFittingTool::initialize() {
  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);
  info() << "TrackFittingTool initialized in placeholder/pass-through mode"
         << endmsg;
  return StatusCode::SUCCESS;
}

StatusCode TrackFittingTool::fitTracks(
    const k4ActsTracking::ActsTrackContainerPtr& inTracks,
    const k4ActsTracking::ActsTrackStateContainerPtr& inTrackStates,
    const k4ActsTracking::ActsTrackContainerPtr& outTracks,
    const k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const {

  if (!inTracks || !inTrackStates || !outTracks || !outTrackStates) {
    error() << "Null input/output container passed to TrackFittingTool"
            << endmsg;
    return StatusCode::FAILURE;
  }

  MutableTrackContainer input(inTracks, inTrackStates);
  MutableTrackContainer output(outTracks, outTrackStates);

  std::size_t nCopied = 0;
  for (const auto& trk : input) {
    auto dst = output.makeTrack();
    dst.copyFrom(trk, true);
    ++nCopied;
  }

  debug() << "TrackFittingTool copied " << nCopied
          << " track(s) into fitted output container" << endmsg;

  return StatusCode::SUCCESS;
}
