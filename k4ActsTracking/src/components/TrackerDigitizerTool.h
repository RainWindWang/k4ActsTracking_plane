#pragma once

#include "k4ActsTracking/ITrackerDigitizerTool.h"
#include "k4ActsTracking/IActsGeoSvc.h"
#include "k4ActsTracking/ITrackerMappingSvc.h"
#include "k4ActsTracking/IDigitizationSvc.h"

#include <GaudiKernel/AlgTool.h>
#include <GaudiKernel/ServiceHandle.h>
#include <Gaudi/Property.h>

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Surfaces/Surface.hpp>

class TrackerDigitizerTool final : public extends<AlgTool, ITrackerDigitizerTool> {
public:
  TrackerDigitizerTool(const std::string& type, const std::string& name, const IInterface* parent);

  StatusCode initialize() override;

  StatusCode digitize(const EventContext& ctx,
                      const edm4hep::SimTrackerHitCollection& in,
                      edm4hep::TrackerHitPlaneCollection& out) const override;

private:
  Gaudi::Property<std::string> m_actsGeoSvcName{this, "ActsGeoSvc", "ActsGeoPlaneSvc", "IActsGeoSvc provider"};
  Gaudi::Property<std::string> m_mappingSvcName{this, "MappingSvc", "TrackerMappingSvc", "ITrackerMappingSvc provider"};
  Gaudi::Property<std::string> m_digitizationSvcName{this, "DigitizationSvc", "DigitizationSvc", "IDigitizationSvc provider"};

  // Minimal model options
  Gaudi::Property<bool> m_useSimHitLocalAsFallback{this, "UseSimHitLocalAsFallback", false,
                                                   "If x/y not present, use SimTrackerHit position projected as fallback (debug)"};

  ServiceHandle<IActsGeoSvc> m_actsGeoSvc;
  ServiceHandle<ITrackerMappingSvc> m_mappingSvc;
  ServiceHandle<IDigitizationSvc> m_digiSvc;

  void buildLocalUV(std::uint64_t cellID, double& u, double& v) const;
  void applyOptionalSmearing(double& u, double& v) const;

  static void fillPlaneFrame(const Acts::Surface& surface,
                             const Acts::GeometryContext& gctx,
                             const Acts::Vector3& gpos,
                             edm4hep::MutableTrackerHitPlane& hit);
};
