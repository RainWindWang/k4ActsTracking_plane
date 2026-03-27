#include "TrackFindingCKFTool.h"

#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/SympyStepper.hpp>
#include <Acts/TrackFinding/CombinatorialKalmanFilter.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/Utilities/Logger.hpp>

DECLARE_COMPONENT(TrackFindingCKFTool)

namespace {

class IndexSourceLink {
public:
  IndexSourceLink(Acts::GeometryIdentifier geoId, std::uint32_t idx)
      : m_geoId(geoId), m_index(idx) {}

  Acts::GeometryIdentifier geometryId() const { return m_geoId; }
  std::uint32_t index() const { return m_index; }

private:
  Acts::GeometryIdentifier m_geoId;
  std::uint32_t m_index;
};

}  // namespace

TrackFindingCKFTool::TrackFindingCKFTool(const std::string& type,
                                         const std::string& name,
                                         const IInterface* parent)
    : extends(type, name, parent) {}

StatusCode TrackFindingCKFTool::initialize() {
  if (m_geoSvc.retrieve().isFailure()) {
    error() << "Failed to retrieve ActsGeoSvc: " << m_geoSvc.name() << endmsg;
    return StatusCode::FAILURE;
  }

  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  info() << "TrackFindingCKFTool initialized with GeoSvc=" << m_geoSvc.name()
         << ", MaxSteps=" << m_maxSteps << endmsg;

  return StatusCode::SUCCESS;
}

std::vector<Acts::SourceLink> TrackFindingCKFTool::buildSourceLinks(
    const k4ActsTracking::MeasurementCollection& measurements) const {
  std::vector<Acts::SourceLink> slinks;
  slinks.reserve(measurements.size());

  for (const auto& m : measurements) {
    IndexSourceLink isl(m.geoId, m.hitIndex);
    slinks.emplace_back(isl);
  }

  return slinks;
}

StatusCode TrackFindingCKFTool::findTracks(
    const Acts::BoundTrackParameters& initialParams,
    const k4ActsTracking::MeasurementCollection& measurements,
    const k4ActsTracking::ActsTrackContainerPtr& tracks,
    const k4ActsTracking::ActsTrackStateContainerPtr& trackStates) const {

  if (!tracks || !trackStates) {
    error() << "Null output track/state container passed to TrackFindingCKFTool"
            << endmsg;
    return StatusCode::FAILURE;
  }

  auto trackingGeometry = m_geoSvc->trackingGeometry();
  auto magneticField = m_geoSvc->magneticField();

  if (!trackingGeometry) {
    error() << "ActsGeoSvc returned null tracking geometry" << endmsg;
    return StatusCode::FAILURE;
  }
  if (!magneticField) {
    error() << "ActsGeoSvc returned null magnetic field provider" << endmsg;
    return StatusCode::FAILURE;
  }

  using Stepper = Acts::SympyStepper;
  using Navigator = Acts::Navigator;
  using Propagator = Acts::Propagator<Stepper, Navigator>;
  using TrackContainer =
      Acts::TrackContainer<Acts::VectorTrackContainer,
                           Acts::VectorMultiTrajectory, std::shared_ptr>;

  using Extensions =
      Acts::CombinatorialKalmanFilterExtensions<TrackContainer>;

  // Build the propagator from geometry + field provided by IActsGeoSvc
  Stepper stepper(std::move(magneticField));

  Acts::Navigator::Config navCfg{trackingGeometry};
  navCfg.resolveSensitive = true;
  navCfg.resolveMaterial = false;
  navCfg.resolvePassive = false;

  Navigator navigator(navCfg);
  Propagator propagator(stepper, navigator);

  // CKF extension
  Acts::GainMatrixUpdater updater;
  Extensions extensions;

  extensions.updater.connect<
      &Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(
      &updater);

  // --------------------------------------------------------------------------
  // Baseline SourceLink + MeasurementSelector wiring
  //
  // IMPORTANT:
  // This is intentionally still a minimal implementation:
  //   - all measurements are exposed as candidates
  //   - no LUXE-specific residual / layer cuts yet
  //
  // The next iteration should replace this by proper TrackStateCreator-style
  // wiring (SourceLinkAccessor + calibrator + measurement selector).
  // --------------------------------------------------------------------------
  auto sourceLinks = buildSourceLinks(measurements);

  extensions.sourceLinkAccessor =
      [&sourceLinks](const auto&, const auto&, Acts::Direction) {
        return sourceLinks;
      };

  extensions.measurementSelector =
      [](const auto&, const auto& cands) {
        return cands;  // baseline: no cuts yet
      };

  Acts::CombinatorialKalmanFilter ckf(propagator, extensions);

  Acts::CombinatorialKalmanFilterOptions options(
      Acts::GeometryContext{},
      Acts::MagneticFieldContext{},
      Acts::CalibrationContext{},
      extensions);

  options.maxSteps = m_maxSteps;

  TrackContainer outTracks(tracks, trackStates);

  auto result = ckf.findTracks(initialParams, options, outTracks);
  if (!result.ok()) {
    warning() << "CKF failed with error code " << result.error() << endmsg;
    return StatusCode::FAILURE;
  }

  debug() << "CKF produced " << result.value().size()
          << " track candidate(s) for one initial parameter" << endmsg;

  return StatusCode::SUCCESS;
}
