#pragma once

#include "k4ActsTracking/ISeedingTool.h"

#include <Gaudi/Property.h>
#include <GaudiKernel/AlgTool.h>

#include <Acts/Definitions/Units.hpp>
#include <Acts/Seeding2/BroadTripletSeedFilter.hpp>
#include <Acts/Seeding2/CylindricalSpacePointKDTree.hpp>
#include <Acts/Seeding2/DoubletSeedFinder.hpp>
#include <Acts/Seeding2/TripletSeedFinder.hpp>
#include <Acts/Utilities/Logger.hpp>
#include <memory>


class SeedingTool final : public extends<AlgTool, ISeedingTool> {
public:
  SeedingTool(const std::string& type, const std::string& name, const IInterface* parent);

  StatusCode initialize() override;

  StatusCode createSeeds(const Acts::SpacePointContainer2& spacePoints,
                         Acts::SeedContainer2& outSeeds) const override;

private:
  struct StraightLineCutConfig {
    bool enable = true;
    float maxCollinearity = 5e-3f;   // |(p2-p1) x (p3-p1)| / (|p2-p1||p3-p1|)
    float maxResidualRphi = 3.0f;    // [mm] optional
    float maxResidualZ = 3.0f;       // [mm]
  };

  // --- Seeding region / KDTree options (like OrthogonalTripletSeedingAlgorithm) ---
  Gaudi::Property<float> m_rMax{this, "rMax", 600.0f, "Max radius [mm]"};
  Gaudi::Property<float> m_zMin{this, "zMin", -4300.0f, "Min z [mm]"};
  Gaudi::Property<float> m_zMax{this, "zMax", +4300.0f, "Max z [mm]"};
  Gaudi::Property<float> m_phiMin{this, "phiMin", -3.2f, "Min phi [rad]"};
  Gaudi::Property<float> m_phiMax{this, "phiMax", +3.2f, "Max phi [rad]"};

  Gaudi::Property<float> m_deltaRMin{this, "deltaRMin", 1.0f, "Min deltaR [mm]"};
  Gaudi::Property<float> m_deltaRMax{this, "deltaRMax", 500.0f, "Max deltaR [mm]"};
  Gaudi::Property<float> m_deltaZMin{this, "deltaZMin", -5000.0f, "Min deltaZ [mm]"};
  Gaudi::Property<float> m_deltaZMax{this, "deltaZMax", +5000.0f, "Max deltaZ [mm]"};

  Gaudi::Property<float> m_impactMax{this, "impactMax", 1000.0f, "Max impact parameter [mm] (loose)"};
  Gaudi::Property<float> m_cotThetaMax{this, "cotThetaMax", 999.0f, "Max |cot(theta)| (loose)"};
  Gaudi::Property<float> m_deltaPhiMax{this, "deltaPhiMax", 0.5f, "Max deltaPhi [rad]"};

  // For B=0 tracker: disable pt/helix cuts by loosening; keep epsilon bField for numerics
  Gaudi::Property<float> m_bFieldInZ{
      this, "bFieldInZ", 1e-9f,
      "Numerical epsilon Bz [T] for helix-based formulas"};
  Gaudi::Property<float> m_minPt{this, "minPt", 0.0f, "Min pT [GeV] (0 => no cut)"};
  Gaudi::Property<float> m_helixCutTolerance{
      this, "helixCutTolerance", 1e6f,
      "Huge => effectively disable helix cut"};
  Gaudi::Property<float> m_toleranceParam{
      this, "toleranceParam", 1.0f,
      "TripletFinder tolerance parameter (keep default-ish)"};

  // Filter config (BroadTripletSeedFilter)
  Gaudi::Property<float> m_deltaInvHelixDiameter{
      this, "deltaInvHelixDiameter", 1e6f,
      "Huge => disable curvature consistency"};
  Gaudi::Property<float> m_compatSeedWeight{this, "compatSeedWeight", 0.0f, ""};
  Gaudi::Property<float> m_impactWeightFactor{this, "impactWeightFactor", 0.0f, ""};
  Gaudi::Property<float> m_zOriginWeightFactor{this, "zOriginWeightFactor", 0.0f, ""};
  Gaudi::Property<unsigned int> m_maxSeedsPerSpM{this, "maxSeedsPerSpM", 50u, ""};

  // Optional: variable middle SP range
  Gaudi::Property<bool> m_useVariableMiddleSPRange{this, "useVariableMiddleSPRange", false, ""};
  Gaudi::Property<float> m_rMinMiddle{this, "rMinMiddle", 0.0f, ""};
  Gaudi::Property<float> m_rMaxMiddle{this, "rMaxMiddle", 1e9f, ""};

  // Straight-line consistency cut (dominant for B=0 tracker)
  Gaudi::Property<bool> m_enableStraightLineCut{this, "enableStraightLineCut", true, ""};
  Gaudi::Property<float> m_maxCollinearity{
      this, "maxCollinearity", 5e-3f,
      "Normalized cross-product magnitude threshold"};

  // Internal ACTS objects
  mutable Acts::BroadTripletSeedFilter::Config m_filterCfg{};
  std::unique_ptr<const Acts::Logger> m_logger;
};
