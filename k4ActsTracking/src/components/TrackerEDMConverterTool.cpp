#include "TrackerEDMConverterTool.h"

#include <k4FWCore/GaudiChecks.h>
#include <fmt/format.h>

#include <array>
#include <cmath>

DECLARE_COMPONENT(TrackerEDMConverterTool)

TrackerEDMConverterTool::TrackerEDMConverterTool(const std::string& type,
                                                 const std::string& name,
                                                 const IInterface* parent)
    : base_class(type, name, parent),
      m_actsGeoSvc(m_actsGeoSvcName.value(), name),
      m_mappingSvc(m_mappingSvcName.value(), name) {}

StatusCode TrackerEDMConverterTool::initialize() {
  K4_GAUDI_CHECK(base_class::initialize());

  m_actsGeoSvc = ServiceHandle<IActsGeoSvc>(m_actsGeoSvcName.value(), name());
  K4_GAUDI_CHECK(m_actsGeoSvc.retrieve());

  m_mappingSvc = ServiceHandle<ITrackerMappingSvc>(m_mappingSvcName.value(), name());
  K4_GAUDI_CHECK(m_mappingSvc.retrieve());

  return StatusCode::SUCCESS;
}

StatusCode TrackerEDMConverterTool::convert(const EventContext& /*ctx*/,
                                            const edm4hep::TrackerHitPlaneCollection& inHits,
                                            k4ActsTracking::SpacePointCollection& outSP,
                                            k4ActsTracking::SourceLinkCollection& outSL,
                                            k4ActsTracking::MeasurementProvider& outMeas) const {
  outSL.clear();
  outSP.sps.clear();

  k4ActsTracking::MeasurementCollection meas;

  outSL.reserve(inHits.size());
  outSP.sps.reserve(inHits.size());
  meas.reserve(inHits.size());

  const Acts::GeometryContext gctx{};
  std::uint32_t idx = 0;

  for (const auto& h : inHits) {
    const std::uint64_t cellID = static_cast<std::uint64_t>(h.getCellID());

    // MappingSvc internally masks x/y if configured
    const Acts::Surface* s = m_mappingSvc->surface(cellID);
    if (!s) {
      ++idx;
      continue;
    }

    const auto p = h.getPosition();
    const Acts::Vector3 gpos(p.x, p.y, p.z);

    auto lp = s->globalToLocal(gctx, gpos, Acts::Vector3(0., 0., 1.));
    if (!lp.ok()) {
      debug() << fmt::format(
          "TrackerEDMConverterTool: globalToLocal failed for cellID={}, skip", cellID)
              << endmsg;
      ++idx;
      continue;
    }

    k4ActsTracking::IndexSourceLink isl;
    isl.geoId = s->geometryId();
    isl.hitIndex = idx;
    outSL.emplace_back(isl);

    auto newSp = outSP.sps.createSpacePoint();

    newSp.assignSourceLinks(std::array<Acts::SourceLink, 1>{
        Acts::SourceLink(&outSL.back())
    });

    // XY / ZR / Phi from hit global position
    const float x = static_cast<float>(p.x);
    const float y = static_cast<float>(p.y);
    const float z = static_cast<float>(p.z);
    const float r = std::hypot(x, y);

    newSp.xy() = std::array<float, 2>{x, y};
    newSp.zr() = std::array<float, 2>{z, r};
    newSp.phi() = static_cast<float>(std::atan2(y, x));

    const float du = h.getDu();
    const float dv = h.getDv();
    const float varU = du * du;
    const float varV = dv * dv;

    // Minimal approximation for Seeding2:
    // map local measurement variances to the SP cylindrical variances
    newSp.varianceR() = varU;
    newSp.varianceZ() = varV;

    k4ActsTracking::TrackerMeasurement2D m;
    m.loc = Acts::Vector2((*lp)[0], (*lp)[1]);
    m.cov.setZero();
    m.cov(0, 0) = varU;
    m.cov(1, 1) = varV;
    m.geoId = s->geometryId();
    m.surface = s;
    m.hitIndex = idx;
    meas.emplace_back(m);

    ++idx;
  }

  outMeas = k4ActsTracking::MeasurementProvider(std::move(meas));

  debug() << fmt::format(
      "TrackerEDMConverterTool: inHits={}, outSP={}, outSL={}, outMeas={}",
      inHits.size(),
      outSP.sps.size(),
      outSL.size(),
      outMeas.measurements().size())
          << endmsg;

  return StatusCode::SUCCESS;
}
