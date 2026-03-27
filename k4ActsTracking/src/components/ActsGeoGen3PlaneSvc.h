#ifndef K4ACTSTRACKING_ACTSGEOGEN3SVC_H
#define K4ACTSTRACKING_ACTSGEOGEN3SVC_H

#include "k4ActsTracking/IActsGeoSvc.h"

#include <Gaudi/Property.h>
#include <k4Interface/IGeoSvc.h>

#include "GaudiKernel/Service.h"

#include <Acts/Calibration/CalibrationContext.hpp>
#include <Acts/Geometry/GeometryContext.hpp>
#include <Acts/MagneticField/MagneticFieldContext.hpp>

#include <memory>
#include <string>

namespace Acts {
  class TrackingGeometry;
  class MagneticFieldProvider;
}  // namespace Acts

namespace dd4hep {
  class Detector;
}

class ActsGeoGen3PlaneSvc : public extends<Service, IActsGeoSvc> {
public:
  ActsGeoGen3PlaneSvc(const std::string& name, ISvcLocator* svcLoc);

  ~ActsGeoGen3PlaneSvc() override = default;

  StatusCode initialize() override;

  std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry() const override;
  std::shared_ptr<const Acts::MagneticFieldProvider> magneticField() const override;

  const Acts::GeometryContext& geometryContext() const override;
  const Acts::MagneticFieldContext& magneticFieldContext() const override;
  const Acts::CalibrationContext& calibrationContext() const override;

  Gaudi::Property<std::string> m_detElementName{
      this, "DetElementName", "Tracker", "Name of the DetElement"};

  Gaudi::Property<std::string> m_layerPattern{
      this, "LayerPatternExpr", "layer", "Layer pattern match expression"};

private:
  dd4hep::Detector*                                  m_dd4hepGeo{nullptr};
  SmartIF<IGeoSvc>                                   m_geoSvc;
  std::shared_ptr<const Acts::TrackingGeometry>      m_trackingGeo{nullptr};
  std::shared_ptr<const Acts::MagneticFieldProvider> m_magneticField{nullptr};

  Acts::GeometryContext      m_geoContext{};
  Acts::MagneticFieldContext m_magFieldContext{};
  Acts::CalibrationContext   m_calibContext{};
};

inline std::shared_ptr<const Acts::TrackingGeometry>
ActsGeoGen3PlaneSvc::trackingGeometry() const {
  return m_trackingGeo;
}

inline std::shared_ptr<const Acts::MagneticFieldProvider>
ActsGeoGen3PlaneSvc::magneticField() const {
  return m_magneticField;
}

inline const Acts::GeometryContext&
ActsGeoGen3PlaneSvc::geometryContext() const {
  return m_geoContext;
}

inline const Acts::MagneticFieldContext&
ActsGeoGen3PlaneSvc::magneticFieldContext() const {
  return m_magFieldContext;
}

inline const Acts::CalibrationContext&
ActsGeoGen3PlaneSvc::calibrationContext() const {
  return m_calibContext;
}

#endif  // K4ACTSTRACKING_ACTSGEOGEN3SVC_H
