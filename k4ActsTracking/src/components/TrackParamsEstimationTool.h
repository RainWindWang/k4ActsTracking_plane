#pragma once

#include "k4ActsTracking/IActsGeoSvc.h"
#include "k4ActsTracking/ITrackParamsEstimationTool.h"

#include <Gaudi/Property.h>
#include <GaudiKernel/AlgTool.h>
#include <GaudiKernel/ServiceHandle.h>

#include <Acts/EventData/ParticleHypothesis.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <memory>
#include <optional>
#include <string>

class TrackParamsEstimationTool final
    : public extends<AlgTool, ITrackParamsEstimationTool> {
public:
  TrackParamsEstimationTool(const std::string& type,
                            const std::string& name,
                            const IInterface* parent);

  StatusCode initialize() override;

  std::optional<Acts::BoundTrackParameters> estimateOneSeed(
      const k4ActsTracking::SpacePointContainer& spacePoints,
      const Acts::SeedContainer2::ConstProxy& seed,
      const k4ActsTracking::MeasurementCollection& measurements) const override;

private:
  Acts::ParticleHypothesis particleHypothesis() const;

private:
  ServiceHandle<IActsGeoSvc> m_geoSvc{
      this, "ActsGeoSvc", "ActsGeoGen3PlaneSvc",
      "Service providing ACTS tracking geometry, magnetic field, and contexts"};

  Gaudi::Property<double> m_assumedMomentumGeV{
      this, "AssumedMomentumGeV", 5.0,
      "Fixed momentum prior [GeV] used to initialize q/p"};

  Gaudi::Property<int> m_charge{
      this, "Charge", +1,
      "Charge sign used for q/p initialization (+1 for positron baseline)"};

  Gaudi::Property<std::string> m_particleHypothesis{
      this, "ParticleHypothesis", "electron",
      "Particle species hypothesis used for BoundTrackParameters; "
      "for positrons use 'electron' with Charge=+1"};

  Gaudi::Property<double> m_sigmaLoc{
      this, "SigmaLoc", 0.05,
      "Initial sigma for loc0/loc1"};

  Gaudi::Property<double> m_sigmaAngle{
      this, "SigmaAngle", 5e-3,
      "Initial sigma for theta/phi [rad]"};

  Gaudi::Property<double> m_sigmaQOverP{
      this, "SigmaQOverP", 0.2,
      "Initial sigma for q/p"};

  std::unique_ptr<const Acts::Logger> m_logger;
};
