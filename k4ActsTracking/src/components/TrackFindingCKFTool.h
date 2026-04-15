#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/IActsGeoSvc.h"

#include <Gaudi/Property.h>
#include <GaudiKernel/AlgTool.h>
#include <GaudiKernel/ServiceHandle.h>

#include <Acts/EventData/SourceLink.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <memory>
#include <vector>

class TrackFindingCKFTool : public extends<GaudiTool, IAlgTool> {
public:
  TrackFindingCKFTool(const std::string& type,
                      const std::string& name,
                      const IInterface* parent);

  StatusCode initialize() override;

  /// Run CKF for one initial parameter
  StatusCode findTracks(
      const Acts::BoundTrackParameters& initialParams,
      const k4ActsTracking::MeasurementCollection& measurements,
      const k4ActsTracking::ActsTrackContainerPtr& tracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& trackStates) const;

private:
  ServiceHandle<IActsGeoSvc> m_geoSvc{
      this, "ActsGeoSvc", "ActsGeoGen3PlaneSvc",
      "Service providing ACTS tracking geometry, magnetic field, and contexts"};

  Gaudi::Property<int> m_maxSteps{
      this, "MaxSteps", 1000,
      "Maximum propagation steps for CKF"};

  std::unique_ptr<const Acts::Logger> m_logger;
};
