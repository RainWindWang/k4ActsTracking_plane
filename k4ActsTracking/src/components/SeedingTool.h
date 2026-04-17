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
#include <vector>

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

  Gaudi::Property<float> m_impactMax{
      this, "impactMax", 1000.0f, "Max impact parameter [mm] (loose)"};
  Gaudi::Property<float> m_cotThetaMax{
      this, "cotThetaMax", 999.0f, "Max |cot(theta)| (loose)"};
  Gaudi::Property<float> m_deltaPhiMax{
      this, "deltaPhiMax", 0.5f, "Max deltaPhi [rad]"};

  // For B=0 tracker: disable pt/helix cuts by loosening; keep epsilon bField for numerics
  Gaudi::Property<float> m_bFieldInZ{
      this, "bFieldInZ", 1e-9f,
      "Numerical epsilon Bz [T] for helix-based formulas"};
  Gaudi::Property<float> m_minPt{
      this, "minPt", 0.0f, "Min pT [GeV] (0 => no cut)"};
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
  Gaudi::Property<float> m_compatSeedWeight{
      this, "compatSeedWeight", 0.0f, ""};
  Gaudi::Property<float> m_impactWeightFactor{
      this, "impactWeightFactor", 0.0f, ""};
  Gaudi::Property<float> m_zOriginWeightFactor{
      this, "zOriginWeightFactor", 0.0f, ""};
  Gaudi::Property<unsigned int> m_maxSeedsPerSpM{
      this, "maxSeedsPerSpM", 50u, ""};

  // Optional: variable middle SP range
  Gaudi::Property<bool> m_useVariableMiddleSPRange{
      this, "useVariableMiddleSPRange", false, ""};
  Gaudi::Property<float> m_rMinMiddle{this, "rMinMiddle", 0.0f, ""};
  Gaudi::Property<float> m_rMaxMiddle{this, "rMaxMiddle", 1e9f, ""};

  // ---------------------------------------------------------------------------
  // Global on/off
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableStraightLineCut{
      this, "enableStraightLineCut", true, ""};

  // ---------------------------------------------------------------------------
  // 0) Seed-layer selection cut (applied first)
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableSeedLayerSelectionCut{
      this, "enableSeedLayerSelectionCut", false,
      "Require seed layers to match configured seedLayers exactly (or reversed if allowed)"};
  Gaudi::Property<std::vector<int>> m_seedLayers{
      this, "seedLayers", std::vector<int>{0, 1, 2},
      "Required seed layers, e.g. [0,1,2]"};
  Gaudi::Property<bool> m_allowReversedSeedLayers{
      this, "allowReversedSeedLayers", true,
      "Allow reversed layer order to match seedLayers, e.g. [2,1,0] for configured [0,1,2]"};

  // ---------------------------------------------------------------------------
  // 1) Collinearity cut
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableCollinearityCut{
      this, "enableCollinearityCut", true,
      "Enable collinearity hard cut"};
  Gaudi::Property<float> m_maxCollinearity{
      this, "maxCollinearity", 5e-3f,
      "Normalized cross-product magnitude threshold"};

  // ---------------------------------------------------------------------------
  // 2) Layer uniqueness + monotonic order
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableLayerOrderCut{
      this, "enableLayerOrderCut", true,
      "Require unique layers and strictly monotonic layer order"};
  Gaudi::Property<bool> m_preferContinuousTriplets{
      this, "preferContinuousTriplets", true,
      "Prefer continuous-layer triplets (e.g. 0-1-2 / 1-2-3) in ranking"};

  // ---------------------------------------------------------------------------
  // 3) Fitted-line residual sanity
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableTripletResidualCut{
      this, "enableTripletResidualCut", true,
      "Enable hard cut on average triplet residual"};
  Gaudi::Property<float> m_tripletResidualMax{
      this, "tripletResidualMax", 0.50f,
      "Max average residual [mm] of the 3-point fitted straight line"};

  // ---------------------------------------------------------------------------
  // 4) 4th-layer support scoring
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableFourthLayerSupportScoring{
      this, "enableFourthLayerSupportScoring", true,
      "Enable extra-layer support search and scoring"};
  Gaudi::Property<float> m_supportResidualMax{
      this, "supportResidualMax", 0.80f,
      "Residual threshold [mm] for counting an extra-layer support point"};

  // quality / scoring weights
  Gaudi::Property<float> m_supportLayerScoreWeight{
      this, "supportLayerScoreWeight", 1000.0f,
      "Score weight per extra supported layer"};
  Gaudi::Property<float> m_supportPointScoreWeight{
      this, "supportPointScoreWeight", 100.0f,
      "Score weight per extra compatible support point"};
  Gaudi::Property<float> m_continuousTripletBonus{
      this, "continuousTripletBonus", 10.0f,
      "Bonus added to quality for continuous-layer triplets"};
  Gaudi::Property<float> m_supportResidualPenalty{
      this, "supportResidualPenalty", 50.0f,
      "Penalty coefficient for support average residual"};
  Gaudi::Property<float> m_tripletResidualPenalty{
      this, "tripletResidualPenalty", 10.0f,
      "Penalty coefficient for triplet average residual"};
  Gaudi::Property<float> m_collinearityPenalty{
      this, "collinearityPenalty", 1.0f,
      "Penalty coefficient for collinearity in seed score"};

  // ---------------------------------------------------------------------------
  // 5) Final family duplicate suppression
  // ---------------------------------------------------------------------------
  Gaudi::Property<bool> m_enableFamilyDuplicateSuppression{
      this, "enableFamilyDuplicateSuppression", true,
      "Enable seed family duplicate suppression after all previous cuts/scoring"};
  Gaudi::Property<float> m_familySlopeBin{
      this, "familySlopeBin", 5.0e-4f,
      "Quantization bin for fitted line slopes ax/ay in family key"};
  Gaudi::Property<float> m_familyInterceptBin{
      this, "familyInterceptBin", 2.0f,
      "Quantization bin [mm] for fitted line intercepts bx/by in family key"};

  // Internal ACTS objects
  mutable Acts::BroadTripletSeedFilter::Config m_filterCfg{};
  std::unique_ptr<const Acts::Logger> m_logger;
};
