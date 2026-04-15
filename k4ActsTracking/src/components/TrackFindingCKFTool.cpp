#include "TrackFindingCKFTool.h"

#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/SympyStepper.hpp>
#include <Acts/TrackFinding/CombinatorialKalmanFilter.hpp>
#include <Acts/TrackFinding/TrackStateCreator.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

DECLARE_COMPONENT(TrackFindingCKFTool)

namespace {

using TrackContainer =
    Acts::TrackContainer<Acts::VectorTrackContainer,
                         Acts::VectorMultiTrajectory,
                         std::shared_ptr>;

class IndexSourceLink {
public:
  IndexSourceLink() = default;

  IndexSourceLink(Acts::GeometryIdentifier geoId, std::uint32_t idx)
      : m_geoId(geoId), m_index(idx) {}

  Acts::GeometryIdentifier geometryId() const { return m_geoId; }
  std::uint32_t index() const { return m_index; }

  bool operator==(const IndexSourceLink& other) const {
    return m_geoId == other.m_geoId && m_index == other.m_index;
  }

private:
  Acts::GeometryIdentifier m_geoId{};
  std::uint32_t m_index{std::numeric_limits<std::uint32_t>::max()};
};

/// --------------------------------------------------------------------------
struct MeasurementLookup {
  std::unordered_map<std::uint32_t, const k4ActsTracking::TrackerMeasurement2D*> byHitIndex;
  std::unordered_map<std::uint64_t, std::vector<Acts::SourceLink>> byGeoId;
};

MeasurementLookup buildMeasurementLookup(
    const k4ActsTracking::MeasurementCollection& measurements) {
  MeasurementLookup lookup;

  for (const auto& m : measurements) {
    lookup.byHitIndex.emplace(m.hitIndex, &m);

    IndexSourceLink isl{m.geoId, m.hitIndex};
    lookup.byGeoId[m.geoId.value()].emplace_back(isl);
  }

  return lookup;
}

/// --------------------------------------------------------------------------
class IndexSourceLinkAccessor {
public:
  using Container = std::unordered_map<std::uint64_t, std::vector<Acts::SourceLink>>;
  using Iterator = std::vector<Acts::SourceLink>::const_iterator;

  const Container* container{nullptr};

  std::pair<Iterator, Iterator> range(const Acts::Surface& surface) const {
    static const std::vector<Acts::SourceLink> kEmpty{};

    if (container == nullptr) {
      return {kEmpty.begin(), kEmpty.end()};
    }

    const auto it = container->find(surface.geometryId().value());
    if (it == container->end()) {
      return {kEmpty.begin(), kEmpty.end()};
    }

    return {it->second.begin(), it->second.end()};
  }
};

class PassThroughMeasurementSelector {
public:
  using Traj = Acts::VectorMultiTrajectory;

  Acts::Result<std::pair<typename std::vector<Traj::TrackStateProxy>::iterator,
                         typename std::vector<Traj::TrackStateProxy>::iterator>>
  select(std::vector<Traj::TrackStateProxy>& candidates,
         bool& isOutlier,
         const Acts::Logger&) const {
    isOutlier = false;
    return std::make_pair(candidates.begin(), candidates.end());
  }
};

/// --------------------------------------------------------------------------
class MeasurementCalibratorAdapter {
public:
  explicit MeasurementCalibratorAdapter(const MeasurementLookup& lookup)
      : m_lookup(lookup) {}

  template <typename track_state_proxy_t>
  Acts::Result<void> calibrate(track_state_proxy_t& trackState) const {
    assert(trackState.hasUncalibratedSourceLink());

    const auto& sourceLink =
        trackState.getUncalibratedSourceLink().template get<IndexSourceLink>();

    const auto it = m_lookup.byHitIndex.find(sourceLink.index());
    if (it == m_lookup.byHitIndex.end() || it->second == nullptr) {
      return Acts::Result<void>::failure(
          std::error_code(static_cast<int>(Acts::CombinatorialKalmanFilterError::UpdateFailed),
                          Acts::CombinatorialKalmanFilterErrorCategory()));
    }

    const auto& meas = *(it->second);

    // Safety check: the SourceLink and measurement should refer to the same surface
    if (meas.geoId != sourceLink.geometryId()) {
      return Acts::Result<void>::failure(
          std::error_code(static_cast<int>(Acts::CombinatorialKalmanFilterError::UpdateFailed),
                          Acts::CombinatorialKalmanFilterErrorCategory()));
    }

    trackState.allocateCalibrated(2);
    trackState.setProjectorSubspaceIndices(
        std::array<Acts::BoundIndices, 2>{Acts::eBoundLoc0, Acts::eBoundLoc1});

    auto calibrated = trackState.template calibrated<2>();
    calibrated.parameters()[0] = meas.loc[0];
    calibrated.parameters()[1] = meas.loc[1];
    calibrated.covariance()    = meas.cov;
    // -----------------------------------------------------------------------

    return Acts::Result<void>::success();
  }

private:
  const MeasurementLookup& m_lookup;
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

  if (!m_geoSvc->trackingGeometry()) {
    error() << "ActsGeoSvc returned null tracking geometry" << endmsg;
    return StatusCode::FAILURE;
  }
  if (!m_geoSvc->magneticField()) {
    error() << "ActsGeoSvc returned null magnetic field provider" << endmsg;
    return StatusCode::FAILURE;
  }

  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  info() << "TrackFindingCKFTool initialized with GeoSvc=" << m_geoSvc.name()
         << ", MaxSteps=" << m_maxSteps << endmsg;

  return StatusCode::SUCCESS;
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
  auto magneticField    = m_geoSvc->magneticField();

  if (!trackingGeometry) {
    error() << "ActsGeoSvc returned null tracking geometry" << endmsg;
    return StatusCode::FAILURE;
  }
  if (!magneticField) {
    error() << "ActsGeoSvc returned null magnetic field provider" << endmsg;
    return StatusCode::FAILURE;
  }

  const auto& gctx = m_geoSvc->geometryContext();
  const auto& mctx = m_geoSvc->magneticFieldContext();
  const auto& cctx = m_geoSvc->calibrationContext();

  using Stepper    = Acts::SympyStepper;
  using Navigator  = Acts::Navigator;
  using Propagator = Acts::Propagator<Stepper, Navigator>;
  using Extensions = Acts::CombinatorialKalmanFilterExtensions<TrackContainer>;
  using TrackStateCreatorType =
      Acts::TrackStateCreator<IndexSourceLinkAccessor::Iterator, TrackContainer>;

  Stepper stepper(std::move(magneticField));

  Acts::Navigator::Config navCfg{trackingGeometry};
  navCfg.resolveSensitive = true;
  navCfg.resolveMaterial  = false;
  navCfg.resolvePassive   = false;

  Navigator navigator(navCfg);
  Propagator propagator(stepper, navigator);

  // --------------------------------------------------------------------------
  auto lookup = buildMeasurementLookup(measurements);

  IndexSourceLinkAccessor slAccessor;
  slAccessor.container = &lookup.byGeoId;

  MeasurementCalibratorAdapter calibrator(lookup);
  PassThroughMeasurementSelector measSel;
  TrackStateCreatorType trackStateCreator;

  trackStateCreator.sourceLinkAccessor
      .template connect<&IndexSourceLinkAccessor::range>(&slAccessor);

  trackStateCreator.calibrator
      .template connect<&MeasurementCalibratorAdapter::calibrate>(&calibrator);

  trackStateCreator.measurementSelector
      .template connect<&PassThroughMeasurementSelector::select>(&measSel);

  Acts::GainMatrixUpdater updater;
  Extensions extensions;

  extensions.updater.connect<
      &Acts::GainMatrixUpdater::operator()<Acts::VectorMultiTrajectory>>(
      &updater);

  extensions.createTrackStates
      .template connect<&TrackStateCreatorType::createTrackStates>(
          &trackStateCreator);

  // --------------------------------------------------------------------------
  Acts::PropagatorPlainOptions propagatorPlainOptions(gctx, mctx);
  propagatorPlainOptions.maxSteps = m_maxSteps;

  Acts::CombinatorialKalmanFilterOptions options(
      gctx, mctx, cctx, extensions, propagatorPlainOptions);

  TrackContainer outTracks(tracks, trackStates);

  Acts::CombinatorialKalmanFilter ckf(
      propagator, logger().cloneWithSuffix("CKF"));

  auto rootBranch = outTracks.makeTrack();
  auto result = ckf.findTracks(initialParams, options, outTracks, rootBranch);

  if (!result.ok()) {
    warning() << "CKF failed with error code " << result.error() << endmsg;
    return StatusCode::FAILURE;
  }

  debug() << "CKF produced " << result.value().size()
          << " track candidate(s) for one initial parameter" << endmsg;

  return StatusCode::SUCCESS;
}
