#include "TrackFindingCKFTool.h"

#include <Acts/Definitions/Direction.hpp>
#include <Acts/EventData/MultiTrajectory.hpp>
#include <Acts/EventData/ProxyAccessor.hpp>
#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/EventData/VectorTrackContainer.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Propagator/Navigator.hpp>
#include <Acts/Propagator/Propagator.hpp>
#include <Acts/Propagator/SympyStepper.hpp>
#include <Acts/Surfaces/PerigeeSurface.hpp>
#include <Acts/TrackFinding/CombinatorialKalmanFilter.hpp>
#include <Acts/TrackFinding/TrackStateCreator.hpp>
#include <Acts/TrackFitting/GainMatrixUpdater.hpp>
#include <Acts/Utilities/TrackHelpers.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

DECLARE_COMPONENT(TrackFindingCKFTool)

namespace {

using TrackStateBackend = k4ActsTracking::ActsTrackStateContainer;
using TrackContainerBackend = k4ActsTracking::ActsTrackContainer;
using MutableTrackContainer =
    Acts::TrackContainer<TrackContainerBackend, TrackStateBackend,
                         std::shared_ptr>;

class PreparedSourceLinkAccessor {
public:
  using Container = std::vector<Acts::SourceLink>;
  using Iterator = Container::const_iterator;

  PreparedSourceLinkAccessor() = default;

  explicit PreparedSourceLinkAccessor(
      const k4ActsTracking::SourceLinkCollection& sourceLinks) {
    prepare(sourceLinks);
  }

  void prepare(const k4ActsTracking::SourceLinkCollection& sourceLinks) {
    m_links.clear();
    m_links.reserve(sourceLinks.size());

    for (const auto& sl : sourceLinks) {
      m_links.emplace_back(Acts::SourceLink{sl});
    }

    std::sort(m_links.begin(), m_links.end(),
              [](const Acts::SourceLink& a, const Acts::SourceLink& b) {
                return a.get<k4ActsTracking::IndexSourceLink>().geoId <
                       b.get<k4ActsTracking::IndexSourceLink>().geoId;
              });
  }

  std::pair<Iterator, Iterator> range(
      const Acts::GeometryIdentifier& geoId) const {
    auto lower = std::lower_bound(
        m_links.begin(), m_links.end(), geoId,
        [](const Acts::SourceLink& sl, const Acts::GeometryIdentifier& gid) {
          return sl.get<k4ActsTracking::IndexSourceLink>().geoId < gid;
        });

    auto upper = std::upper_bound(
        m_links.begin(), m_links.end(), geoId,
        [](const Acts::GeometryIdentifier& gid, const Acts::SourceLink& sl) {
          return gid < sl.get<k4ActsTracking::IndexSourceLink>().geoId;
        });

    return {lower, upper};
  }

private:
  Container m_links;
};

class PassThroughMeasurementCalibrator {
public:
  explicit PassThroughMeasurementCalibrator(
      const k4ActsTracking::MeasurementCollection& measurements)
      : m_measurements(&measurements) {}

  void calibrate(const Acts::GeometryContext& /*gctx*/,
                 const Acts::CalibrationContext& /*cctx*/,
                 const Acts::SourceLink& sourceLink,
                 TrackStateBackend::TrackStateProxy trackState) const {
    Acts::SourceLink sl = sourceLink;
    trackState.setUncalibratedSourceLink(std::move(sl));

    const auto& idxSourceLink =
        sourceLink.get<k4ActsTracking::IndexSourceLink>();

    assert(m_measurements != nullptr);
    assert(idxSourceLink.hitIndex < m_measurements->size());

    const auto& meas = m_measurements->at(idxSourceLink.hitIndex);

    // Minimal consistency guard.
    assert(meas.geoId == idxSourceLink.geoId);

    trackState.allocateCalibrated(meas.loc, meas.cov);

    const std::array<Acts::BoundIndices, 2> indices = {Acts::eBoundLoc0,
                                                       Acts::eBoundLoc1};
    trackState.setProjectorSubspaceIndices(indices);
  }

private:
  const k4ActsTracking::MeasurementCollection* m_measurements = nullptr;
};

class MeasurementSelectorWrapper {
public:
  using Traj = TrackStateBackend;

  explicit MeasurementSelectorWrapper(Acts::MeasurementSelector selector)
      : m_selector(std::move(selector)) {}

  Acts::Result<std::pair<std::vector<Traj::TrackStateProxy>::iterator,
                         std::vector<Traj::TrackStateProxy>::iterator>>
  select(std::vector<Traj::TrackStateProxy>& candidates, bool& isOutlier,
         const Acts::Logger& logger) const {
    return m_selector.select<TrackStateBackend>(candidates, isOutlier, logger);
  }

private:
  Acts::MeasurementSelector m_selector;
};

}  // namespace

TrackFindingCKFTool::TrackFindingCKFTool(const std::string& type,
                                         const std::string& name,
                                         const IInterface* parent)
    : extends<AlgTool, ITrackFindingTool>(type, name, parent) {}

StatusCode TrackFindingCKFTool::initialize() {
  if (m_geoSvc.retrieve().isFailure()) {
    error() << "Failed to retrieve ActsGeoSvc " << m_geoSvc.name() << endmsg;
    return StatusCode::FAILURE;
  }

  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  info() << "Initialized TrackFindingCKFTool with Chi2CutOff=" << m_chi2CutOff
         << ", NumMeasurementsCutOff=" << m_numMeasurementsCutOff
         << ", MaxSteps=" << m_maxSteps
         << ", ReverseSearch=" << (m_reverseSearch ? "true" : "false")
         << endmsg;

  return StatusCode::SUCCESS;
}

StatusCode TrackFindingCKFTool::findTracks(
    const k4ActsTracking::InitialTrackParametersCollection& initialParameters,
    const k4ActsTracking::SourceLinkCollection& sourceLinks,
    const k4ActsTracking::MeasurementCollection& measurements,
    k4ActsTracking::ActsTrackContainerPtr& outTracks,
    k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const {
  if (!outTracks) {
    outTracks = std::make_shared<k4ActsTracking::ActsTrackContainer>();
  }
  if (!outTrackStates) {
    outTrackStates = std::make_shared<k4ActsTracking::ActsTrackStateContainer>();
  }

  if (initialParameters.empty()) {
    debug() << "No initial parameters provided; producing empty track output"
            << endmsg;
    return StatusCode::SUCCESS;
  }

  // --- Geometry / field / context ---
  const auto trackingGeometry = m_geoSvc->trackingGeometry();
  const auto magneticField = m_geoSvc->magneticField();
  const auto& geoCtx = m_geoSvc->geometryContext();
  const auto& magCtx = m_geoSvc->magneticFieldContext();
  const auto& calibCtx = m_geoSvc->calibrationContext();

  if (!trackingGeometry) {
    error() << "TrackingGeometry is null" << endmsg;
    return StatusCode::FAILURE;
  }
  if (!magneticField) {
    error() << "MagneticFieldProvider is null" << endmsg;
    return StatusCode::FAILURE;
  }

  // --- Prepare source links + calibrator + measurement selector ---
  PreparedSourceLinkAccessor slAccessor(sourceLinks);
  PassThroughMeasurementCalibrator calibrator(measurements);

  Acts::MeasurementSelector::Config measurementSelectorCfg = {
      {Acts::GeometryIdentifier(),
       {{}, {m_chi2CutOff.value()},
        {static_cast<std::size_t>(m_numMeasurementsCutOff.value())}}}};

  MeasurementSelectorWrapper measSel{
      Acts::MeasurementSelector(measurementSelectorCfg)};

  using TrackStateCreatorType =
      Acts::TrackStateCreator<PreparedSourceLinkAccessor::Iterator,
                              MutableTrackContainer>;
  TrackStateCreatorType trackStateCreator;
  trackStateCreator.sourceLinkAccessor
      .template connect<&PreparedSourceLinkAccessor::range>(&slAccessor);
  trackStateCreator.calibrator
      .template connect<&PassThroughMeasurementCalibrator::calibrate>(
          &calibrator);
  trackStateCreator.measurementSelector
      .template connect<&MeasurementSelectorWrapper::select>(&measSel);

  Acts::GainMatrixUpdater updater;

  using Extensions = Acts::CombinatorialKalmanFilterExtensions<
      MutableTrackContainer>;
  Extensions extensions;
  extensions.updater.connect<&Acts::GainMatrixUpdater::operator()<
      typename MutableTrackContainer::TrackStateContainerBackend>>(&updater);
  extensions.createTrackStates
      .template connect<&TrackStateCreatorType::createTrackStates>(
          &trackStateCreator);

  // --- Propagator / CKF ---
  Acts::Navigator::Config navCfg{trackingGeometry};
  navCfg.resolvePassive = m_resolvePassive;
  navCfg.resolveMaterial = m_resolveMaterial;
  navCfg.resolveSensitive = m_resolveSensitive;

  using Stepper = Acts::SympyStepper;
  using Navigator = Acts::Navigator;
  using Propagator = Acts::Propagator<Stepper, Navigator>;
  using CKF = Acts::CombinatorialKalmanFilter<Propagator, MutableTrackContainer>;
  using TrackFinderOptions =
      Acts::CombinatorialKalmanFilterOptions<MutableTrackContainer>;

  Stepper stepper(magneticField);
  Navigator navigator(navCfg, m_logger->cloneWithSuffix("Navigator"));
  Propagator propagator(std::move(stepper), std::move(navigator),
                        m_logger->cloneWithSuffix("Propagator"));
  CKF trackFinder(std::move(propagator), m_logger->cloneWithSuffix("CKF"));

  Acts::PropagatorPlainOptions propOptions(geoCtx, magCtx);
  propOptions.maxSteps = m_maxSteps;
  propOptions.direction = m_reverseSearch ? Acts::Direction::Backward()
                                          : Acts::Direction::Forward();

  auto perigeeSurface = Acts::Surface::makeShared<Acts::PerigeeSurface>(
      Acts::Vector3{0., 0., 0.});

  TrackFinderOptions options(geoCtx, magCtx, calibCtx, extensions, propOptions);
  options.targetSurface = m_reverseSearch ? perigeeSurface.get() : nullptr;

  // --- Final + temporary containers ---
  MutableTrackContainer finalTracks(outTracks, outTrackStates);

  auto tempTrackContainer =
      std::make_shared<k4ActsTracking::ActsTrackContainer>();
  auto tempTrackStates =
      std::make_shared<k4ActsTracking::ActsTrackStateContainer>();
  MutableTrackContainer tempTracks(tempTrackContainer, tempTrackStates);

  std::size_t nFound = 0;
  std::size_t nFailed = 0;

  for (const auto& initialParams : initialParameters) {
    tempTracks.clear();

    auto rootBranch = tempTracks.makeTrack();
    auto result =
        trackFinder.findTracks(initialParams, options, tempTracks, rootBranch);

    if (!result.ok()) {
      ++nFailed;
      warning() << "CKF failed for one initial parameter with error "
                << result.error() << endmsg;
      continue;
    }

    for (auto& track : result.value()) {
      if (m_trimTracks) {
        Acts::trimTrack(track, true, true, true, true);
      }
      Acts::calculateTrackQuantities(track);

      auto dest = finalTracks.makeTrack();
      dest.copyFrom(track);
      ++nFound;
    }
  }

  debug() << "Track finding finished: found=" << nFound
          << ", failedSeeds=" << nFailed << endmsg;

  return StatusCode::SUCCESS;
}
