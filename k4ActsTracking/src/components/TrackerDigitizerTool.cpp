#include "TrackerDigitizerTool.h"

#include <k4FWCore/GaudiChecks.h>
#include <GaudiKernel/IRndmGenSvc.h>
#include <GaudiKernel/RndmGenerators.h>

#include "edm4hep/MutableTrackerHitPlane.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

DECLARE_COMPONENT(TrackerDigitizerTool)

namespace {

edm4hep::Vector2f toSpherical(const Acts::Vector3& dir) {
  const double norm = dir.norm();
  if (norm <= 0.) {
    return edm4hep::Vector2f{0.f, 0.f};
  }

  const Acts::Vector3 n = dir / norm;
  const double z = std::clamp(static_cast<double>(n.z()), -1.0, 1.0);
  const float theta = static_cast<float>(std::acos(z));
  const float phi = static_cast<float>(std::atan2(n.y(), n.x()));
  return edm4hep::Vector2f{theta, phi};
}

edm4hep::CovMatrix3f buildGlobalCovariance(const Acts::Vector3& uDir,
                                           const Acts::Vector3& vDir,
                                           double varU,
                                           double varV) {
  const Acts::Vector3 u = uDir.norm() > 0. ? uDir.normalized() : Acts::Vector3::Zero();
  const Acts::Vector3 v = vDir.norm() > 0. ? vDir.normalized() : Acts::Vector3::Zero();

  const double xx = varU * u.x() * u.x() + varV * v.x() * v.x();
  const double xy = varU * u.x() * u.y() + varV * v.x() * v.y();
  const double yy = varU * u.y() * u.y() + varV * v.y() * v.y();
  const double xz = varU * u.x() * u.z() + varV * v.x() * v.z();
  const double yz = varU * u.y() * u.z() + varV * v.y() * v.z();
  const double zz = varU * u.z() * u.z() + varV * v.z() * v.z();

  // Lower-triangular storage order for CovMatrix3f:
  // [xx, xy, yy, xz, yz, zz]
  return edm4hep::CovMatrix3f{
      static_cast<float>(xx),
      static_cast<float>(xy),
      static_cast<float>(yy),
      static_cast<float>(xz),
      static_cast<float>(yz),
      static_cast<float>(zz)};
}

} // namespace

TrackerDigitizerTool::TrackerDigitizerTool(const std::string& type,
                                           const std::string& name,
                                           const IInterface* parent)
    : base_class(type, name, parent),
      m_actsGeoSvc(m_actsGeoSvcName.value(), name),
      m_mappingSvc(m_mappingSvcName.value(), name),
      m_digiSvc(m_digitizationSvcName.value(), name) {}

StatusCode TrackerDigitizerTool::initialize() {
  K4_GAUDI_CHECK(base_class::initialize());

  m_actsGeoSvc = ServiceHandle<IActsGeoSvc>(m_actsGeoSvcName.value(), name());
  K4_GAUDI_CHECK(m_actsGeoSvc.retrieve());

  m_mappingSvc = ServiceHandle<ITrackerMappingSvc>(m_mappingSvcName.value(), name());
  K4_GAUDI_CHECK(m_mappingSvc.retrieve());

  m_digiSvc = ServiceHandle<IDigitizationSvc>(m_digitizationSvcName.value(), name());
  K4_GAUDI_CHECK(m_digiSvc.retrieve());

  info() << fmt::format("TrackerDigitizerTool: ActsGeoSvc='{}', MappingSvc='{}', DigitizationSvc='{}'",
                        m_actsGeoSvcName.value(), m_mappingSvcName.value(), m_digitizationSvcName.value())
         << endmsg;

  return StatusCode::SUCCESS;
}

void TrackerDigitizerTool::buildLocalUV(std::uint64_t cellID, double& u, double& v) const {
  // Minimal pixel-index -> local coordinate model:
  // u = (x + 0.5)*pitchX + offsetX
  // v = (y + 0.5)*pitchY + offsetY
  const auto xOpt = m_digiSvc->decodeX(cellID);
  const auto yOpt = m_digiSvc->decodeY(cellID);

  const int x = xOpt.value_or(0);
  const int y = yOpt.value_or(0);

  u = (static_cast<double>(x) + 0.5) * m_digiSvc->pitchX() + m_digiSvc->offsetX();
  v = (static_cast<double>(y) + 0.5) * m_digiSvc->pitchY() + m_digiSvc->offsetY();
}

void TrackerDigitizerTool::applyOptionalSmearing(double& u, double& v) const {
  const double su = m_digiSvc->smearSigmaU();
  const double sv = m_digiSvc->smearSigmaV();
  if (su == 0.0 && sv == 0.0) {
    return;
  }

  IRndmGenSvc* rndm = m_digiSvc->rndmSvc();
  if (!rndm) {
    return;
  }

  if (su > 0.0) {
    Rndm::Numbers gaussU(rndm, Rndm::Gauss(0.0, su));
    u += gaussU();
  }
  if (sv > 0.0) {
    Rndm::Numbers gaussV(rndm, Rndm::Gauss(0.0, sv));
    v += gaussV();
  }
}

void TrackerDigitizerTool::fillPlaneFrame(const Acts::Surface& surface,
                                          const Acts::GeometryContext& gctx,
                                          const Acts::Vector3& gpos,
                                          edm4hep::MutableTrackerHitPlane& hit) {
  // Global measurement axes derived from the ACTS surface reference frame.
  const Acts::RotationMatrix3 rf =
      surface.referenceFrame(gctx, gpos, Acts::Vector3(0., 0., 1.));

  const Acts::Vector3 uDir = rf.col(0);
  const Acts::Vector3 vDir = rf.col(1);

  // EDM4hep stores these directions as (theta, phi) in spherical coordinates.
  hit.setU(toSpherical(uDir));
  hit.setV(toSpherical(vDir));
}

StatusCode TrackerDigitizerTool::digitize(const EventContext& /*ctx*/,
                                          const edm4hep::SimTrackerHitCollection& in,
                                          edm4hep::TrackerHitPlaneCollection& out) const {
  // Simple LUXE digitization: no clustering, no threshold for now.
  const Acts::GeometryContext gctx{};

  for (const auto& simHit : in) {
    const std::uint64_t cellID = simHit.getCellID();

    const Acts::Surface* surface = m_mappingSvc->surface(cellID);
    if (!surface) {
      debug() << fmt::format("TrackerDigitizerTool: no surface for cellID={}, skip", cellID) << endmsg;
      continue;
    }

    double u = 0.0;
    double v = 0.0;

    if (m_digiSvc->hasX() || m_digiSvc->hasY()) {
      buildLocalUV(cellID, u, v);
    } else if (m_useSimHitLocalAsFallback.value()) {
      // Fallback: project the SimTrackerHit global position to the surface local frame.
      const auto p = simHit.getPosition();
      const Acts::Vector3 simGpos(p.x, p.y, p.z);
      auto lp = surface->globalToLocal(gctx, simGpos, Acts::Vector3(0., 0., 1.));
      if (lp.ok()) {
        u = (*lp)[0];
        v = (*lp)[1];
      } else {
        debug() << fmt::format("TrackerDigitizerTool: globalToLocal failed for cellID={}, skip", cellID) << endmsg;
        continue;
      }
    } else {
      debug() << fmt::format("TrackerDigitizerTool: no local decoding and fallback disabled for cellID={}, skip", cellID)
              << endmsg;
      continue;
    }

    applyOptionalSmearing(u, v);

    const Acts::Vector2 lpos(u, v);
    const Acts::Vector3 gpos =
        surface->localToGlobal(gctx, lpos, Acts::Vector3(0., 0., 1.));

    auto hit = out.create();

    hit.setCellID(cellID);
    hit.setQuality(0);
    hit.setTime(simHit.getTime());
    hit.setEDep(simHit.getEDep());

    hit.setPosition(edm4hep::Vector3d{
        static_cast<double>(gpos.x()),
        static_cast<double>(gpos.y()),
        static_cast<double>(gpos.z())});

    // Measurement resolution along the local surface axes.
    hit.setDu(static_cast<float>(m_digiSvc->smearSigmaU() > 0.0
                                     ? m_digiSvc->smearSigmaU()
                                     : std::sqrt(m_digiSvc->varU())));
    hit.setDv(static_cast<float>(m_digiSvc->smearSigmaV() > 0.0
                                     ? m_digiSvc->smearSigmaV()
                                     : std::sqrt(m_digiSvc->varV())));

    fillPlaneFrame(*surface, gctx, gpos, hit);

    // Build a global 3x3 covariance from the local measurement variances.
    const Acts::RotationMatrix3 rf =
        surface->referenceFrame(gctx, gpos, Acts::Vector3(0., 0., 1.));
    const Acts::Vector3 uDir = rf.col(0);
    const Acts::Vector3 vDir = rf.col(1);

    hit.setCovMatrix(buildGlobalCovariance(uDir, vDir,
                                           m_digiSvc->varU(),
                                           m_digiSvc->varV()));
  }

  debug() << fmt::format("TrackerDigitizerTool: in={}, out={}", in.size(), out.size()) << endmsg;
  return StatusCode::SUCCESS;
}
