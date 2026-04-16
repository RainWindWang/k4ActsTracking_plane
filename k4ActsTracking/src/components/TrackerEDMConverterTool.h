#pragma once

#include "k4ActsTracking/ITrackerEDMConverterTool.h"
#include "k4ActsTracking/ITrackerMappingSvc.h"
#include "k4ActsTracking/IActsGeoSvc.h"

#include <GaudiKernel/AlgTool.h>
#include <GaudiKernel/ServiceHandle.h>
#include <Gaudi/Property.h>

class TrackerEDMConverterTool final : public extends<AlgTool, ITrackerEDMConverterTool> {
public:
  TrackerEDMConverterTool(const std::string& type, const std::string& name, const IInterface* parent);

  StatusCode initialize() override;

  StatusCode convert(const EventContext& ctx,
                     const edm4hep::TrackerHitPlaneCollection& inHits,
                     k4ActsTracking::SpacePointCollection& outSP,
                     k4ActsTracking::SourceLinkCollection& outSL,
                     k4ActsTracking::MeasurementCollection& outMeas) const override;

private:
  Gaudi::Property<std::string> m_actsGeoSvcName{this, "ActsGeoSvc", "ActsGeoPlaneSvc", "IActsGeoSvc provider"};
  Gaudi::Property<std::string> m_mappingSvcName{this, "MappingSvc", "TrackerMappingSvc", "ITrackerMappingSvc provider"};

  ServiceHandle<IActsGeoSvc> m_actsGeoSvc;
  ServiceHandle<ITrackerMappingSvc> m_mappingSvc;
};
