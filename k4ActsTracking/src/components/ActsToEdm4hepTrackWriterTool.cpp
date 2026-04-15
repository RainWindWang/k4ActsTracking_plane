#include "ActsToEdm4hepTrackWriterTool.h"

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <edm4hep/MutableTrack.h>
#include <edm4hep/MutableTrackState.h>
#include <edm4hep/TrackState.h>
#include <edm4hep/Vector3f.h>

#include <array>
#include <cmath>
#include <memory>

DECLARE_COMPONENT(ActsToEdm4hepTrackWriterTool)

namespace {
using MutableTrackContainer =
    Acts::TrackContainer<Acts::VectorTrackContainer,
                         Acts::VectorMultiTrajectory,
                         std::shared_ptr>;

std::array<float, 21> packCov6x6LowerTriangular(
    const Acts::BoundSquareMatrix& cov) {
  std::array<float, 21> out{};
  std::size_t k = 0;
  for (std::size_t i = 0; i < 6; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      out[k++] = static_cast<float>(cov(i, j));
    }
  }
  return out;
}

float safeOmegaFromQOverPTheta(double qOverP, double theta) {
  const double sinTheta = std::sin(theta);
  if (std::abs(sinTheta) < 1e-12) {
    return 0.f;
  }
  return static_cast<float>(qOverP / sinTheta);
}

float safeTanLambdaFromTheta(double theta) {
  const double tanTheta = std::tan(theta);
  if (std::abs(tanTheta) < 1e-12) {
    return 0.f;
  }
  return static_cast<float>(1.0 / tanTheta);
}

}  // namespace

ActsToEdm4hepTrackWriterTool::ActsToEdm4hepTrackWriterTool(
    const std::string& type, const std::string& name, const IInterface* parent)
    : extends(type, name, parent) {}

StatusCode ActsToEdm4hepTrackWriterTool::initialize() {
  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);
  info() << "ActsToEdm4hepTrackWriterTool initialized in minimal writer mode"
         << endmsg;
  return StatusCode::SUCCESS;
}

StatusCode ActsToEdm4hepTrackWriterTool::write(
    const k4ActsTracking::ActsTrackContainerPtr& inTracks,
    const k4ActsTracking::ActsTrackStateContainerPtr& inTrackStates,
    edm4hep::TrackCollection& outTracks,
    edm4hep::TrackStateCollection& outTrackStates) const {

  if (!inTracks || !inTrackStates) {
    error() << "Null ACTS input containers passed to EDM4hep writer"
            << endmsg;
    return StatusCode::FAILURE;
  }

  MutableTrackContainer tracks(inTracks, inTrackStates);

  std::size_t nWritten = 0;

  for (const auto& trk : tracks) {
    auto edmTrack = outTracks.create();

    // Minimal track-level bookkeeping
    edmTrack.setType(0);
    edmTrack.setChi2(0.f);
    edmTrack.setNdf(0);

    if (trk.hasReferenceSurface()) {
      const auto pars = trk.parameters();
      const auto cov  = trk.covariance();

      auto edmState = outTrackStates.create();

      edmState.setLocation(static_cast<int>(edm4hep::TrackState::AtOther));
      edmState.setD0(static_cast<float>(pars[Acts::eBoundLoc0]));
      edmState.setZ0(static_cast<float>(pars[Acts::eBoundLoc1]));
      edmState.setPhi(static_cast<float>(pars[Acts::eBoundPhi]));
      edmState.setTanLambda(
          safeTanLambdaFromTheta(pars[Acts::eBoundTheta]));
      edmState.setOmega(
          safeOmegaFromQOverPTheta(pars[Acts::eBoundQOverP],
                                   pars[Acts::eBoundTheta]));
      edmState.setTime(0.f);
      edmState.setReferencePoint(edm4hep::Vector3f{0.f, 0.f, 0.f});
      edmState.setCovMatrix(packCov6x6LowerTriangular(cov));

      // Minimal relation: one representative state per track
      edmTrack.addToTrackStates(edmState);
    }

    ++nWritten;
  }

  debug() << "ActsToEdm4hepTrackWriterTool wrote " << nWritten
          << " EDM4hep track(s)" << endmsg;

  return StatusCode::SUCCESS;
}
