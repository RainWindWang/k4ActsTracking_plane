#include "TrackParamsEstimationTool.h"

#include <Acts/Definitions/TrackParametrization.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/EventData/ParticleHypothesis.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

DECLARE_COMPONENT(TrackParamsEstimationTool)

namespace {

Acts::Vector3 spPos(const k4ActsTracking::SpacePointContainer::ConstProxy& sp) {
  const auto xy = sp.xy();
  const auto zr = sp.zr();
  return {xy[0], xy[1], zr[0]};
}

const k4ActsTracking::TrackerMeasurement2D* findMeas(
    const k4ActsTracking::MeasurementCollection& meas,
    std::uint32_t idx) {
  for (const auto& m : meas) {
    if (m.hitIndex == idx) {
      return &m;
    }
  }
  return nullptr;
}

std::optional<std::uint32_t> firstHitIndex(
    const k4ActsTracking::SpacePointContainer::ConstProxy& sp) {
  const auto slRange = sp.sourceLinks();
  if (slRange.empty() || slRange[0].get() == nullptr) {
    return std::nullopt;
  }
  return slRange[0].get()->hitIndex;
}

}  // namespace

TrackParamsEstimationTool::TrackParamsEstimationTool(
    const std::string& type, const std::string& name, const IInterface* parent)
    : extends(type, name, parent) {}

StatusCode TrackParamsEstimationTool::initialize() {
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

  if (m_assumedMomentumGeV <= 0.) {
    error() << "AssumedMomentumGeV must be > 0" << endmsg;
    return StatusCode::FAILURE;
  }
  if (m_charge != +1 && m_charge != -1) {
    error() << "Charge must be +1 or -1" << endmsg;
    return StatusCode::FAILURE;
  }

  (void)particleHypothesis();

  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  info() << "TrackParamsEstimationTool initialized with ActsGeoSvc="
         << m_geoSvc.name()
         << ", AssumedMomentumGeV=" << m_assumedMomentumGeV
         << ", Charge=" << m_charge
         << ", ParticleHypothesis=" << m_particleHypothesis << endmsg;

  return StatusCode::SUCCESS;
}

Acts::ParticleHypothesis TrackParamsEstimationTool::particleHypothesis() const {
  if (m_particleHypothesis == "electron") {
    return Acts::ParticleHypothesis::electron();
  }

  warning() << "Unknown ParticleHypothesis='" << m_particleHypothesis
            << "', falling back to electron()" << endmsg;
  return Acts::ParticleHypothesis::electron();
}

std::optional<Acts::BoundTrackParameters>
TrackParamsEstimationTool::estimateOneSeed(
    const k4ActsTracking::SpacePointContainer& spacePoints,
    const Acts::SeedContainer2::ConstProxy& seed,
    const k4ActsTracking::MeasurementCollection& measurements) const {

  const auto& gctx = m_geoSvc->geometryContext();

  const auto idx = seed.spacePointIndices();

  if (idx[0] >= spacePoints.size() || idx[1] >= spacePoints.size() ||
      idx[2] >= spacePoints.size()) {
    return std::nullopt;
  }

  const auto sp0 = spacePoints.at(idx[0]);
  const auto sp1 = spacePoints.at(idx[1]);
  const auto sp2 = spacePoints.at(idx[2]);

  const Acts::Vector3 p0 = spPos(sp0);
  const Acts::Vector3 p1 = spPos(sp1);
  const Acts::Vector3 p2 = spPos(sp2);

  (void)p1;
  Acts::Vector3 dir = p2 - p0;
  if (dir.norm() < 1e-6) {
    return std::nullopt;
  }
  dir = dir.normalized();

  const auto hitIndexOpt = firstHitIndex(sp0);
  if (!hitIndexOpt.has_value()) {
    return std::nullopt;
  }

  const auto* meas = findMeas(measurements, *hitIndexOpt);
  if (!meas || !meas->surface) {
    return std::nullopt;
  }

  auto local = meas->surface->globalToLocal(gctx, p0, dir);
  if (!local.ok()) {
    return std::nullopt;
  }

  const double theta = std::acos(std::clamp(dir.z(), -1.0, 1.0));
  const double phi   = std::atan2(dir.y(), dir.x());

  const double qop =
      static_cast<double>(m_charge) /
      (m_assumedMomentumGeV * Acts::UnitConstants::GeV);

  Acts::BoundVector pars = Acts::BoundVector::Zero();
  pars[Acts::eBoundLoc0]   = (*local)[0];
  pars[Acts::eBoundLoc1]   = (*local)[1];
  pars[Acts::eBoundQOverP] = qop;
  pars[Acts::eBoundTheta]  = theta;
  pars[Acts::eBoundPhi]    = phi;

  Acts::BoundSquareMatrix cov = Acts::BoundSquareMatrix::Zero();
  cov(Acts::eBoundLoc0, Acts::eBoundLoc0) = m_sigmaLoc * m_sigmaLoc;
  cov(Acts::eBoundLoc1, Acts::eBoundLoc1) = m_sigmaLoc * m_sigmaLoc;
  cov(Acts::eBoundTheta, Acts::eBoundTheta) = m_sigmaAngle * m_sigmaAngle;
  cov(Acts::eBoundPhi, Acts::eBoundPhi)     = m_sigmaAngle * m_sigmaAngle;
  cov(Acts::eBoundQOverP, Acts::eBoundQOverP) =
      m_sigmaQOverP * m_sigmaQOverP;

  return Acts::BoundTrackParameters(
      meas->surface->getSharedPtr(), pars, cov, particleHypothesis());
}
