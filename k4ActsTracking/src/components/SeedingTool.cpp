#include "SeedingTool.h"

#include <Acts/Definitions/Direction.hpp>
#include <Acts/Seeding2/CylindricalSpacePointKDTree.hpp>
#include <Acts/Seeding2/TripletSeeder.hpp>
#include <Acts/Utilities/Helpers.hpp>

#include <cmath>

DECLARE_COMPONENT(SeedingTool)

namespace {
static inline float collinearity(const Acts::Vector3& a,
                                 const Acts::Vector3& b,
                                 const Acts::Vector3& c) {
  // returns |(b-a) x (c-a)| / (|b-a| |c-a|)  (0 => perfectly collinear)
  const Acts::Vector3 u = b - a;
  const Acts::Vector3 v = c - a;
  const double nu = u.norm();
  const double nv = v.norm();
  if (nu == 0.0 || nv == 0.0) {
    return 1e9f;
  }
  const Acts::Vector3 cx = u.cross(v);
  return static_cast<float>(cx.norm() / (nu * nv));
}
}  // namespace

SeedingTool::SeedingTool(const std::string& type, const std::string& name,
                         const IInterface* parent)
    : extends(type, name, parent) {}

StatusCode SeedingTool::initialize() {
  m_logger = Acts::getDefaultLogger(name(), Acts::Logging::INFO);

  m_filterCfg.deltaInvHelixDiameter = m_deltaInvHelixDiameter;
  m_filterCfg.deltaRMin = m_deltaRMin;
  m_filterCfg.compatSeedWeight = m_compatSeedWeight;
  m_filterCfg.impactWeightFactor = m_impactWeightFactor;
  m_filterCfg.zOriginWeightFactor = m_zOriginWeightFactor;
  m_filterCfg.maxSeedsPerSpM = m_maxSeedsPerSpM;

  info() << "Initialized SeedingTool with Bz(eps)=" << m_bFieldInZ
         << "T, minPt=" << m_minPt
         << "GeV, helixCutTolerance=" << m_helixCutTolerance
         << ", straightLineCut=" << (m_enableStraightLineCut ? "ON" : "OFF")
         << endmsg;

  return StatusCode::SUCCESS;
}

StatusCode SeedingTool::createSeeds(
    const Acts::SpacePointContainer2& coreSpacePoints,
    Acts::SeedContainer2& outSeeds) const {
  outSeeds.clear();

  // Build KDTree from the passed-in ACTS space points
  Acts::Experimental::CylindricalSpacePointKDTreeBuilder kdTreeBuilder;
  kdTreeBuilder.reserve(coreSpacePoints.size());

  for (Acts::SpacePointIndex2 i = 0; i < coreSpacePoints.size(); ++i) {
    auto sp = coreSpacePoints.at(i);
    kdTreeBuilder.insert(i, sp.phi(), sp.zr()[1], sp.zr()[0]);
  }

  Acts::Experimental::CylindricalSpacePointKDTree kdTree = kdTreeBuilder.build();

  Acts::Experimental::CylindricalSpacePointKDTree::Options lhOptions;
  lhOptions.rMax = m_rMax;
  lhOptions.zMin = m_zMin;
  lhOptions.zMax = m_zMax;
  lhOptions.phiMin = m_phiMin;
  lhOptions.phiMax = m_phiMax;
  lhOptions.deltaRMin = m_deltaRMin;
  lhOptions.deltaRMax = m_deltaRMax;
  lhOptions.collisionRegionMin = -1e9f;
  lhOptions.collisionRegionMax = +1e9f;
  lhOptions.cotThetaMax = m_cotThetaMax;
  lhOptions.deltaPhiMax = m_deltaPhiMax;

  Acts::Experimental::CylindricalSpacePointKDTree::Options hlOptions = lhOptions;

  Acts::DoubletSeedFinder::Config bottomCfg;
  bottomCfg.spacePointsSortedByRadius = false;
  bottomCfg.candidateDirection = Acts::Direction::Backward();
  bottomCfg.deltaRMin = m_deltaRMin;
  bottomCfg.deltaRMax = m_deltaRMax;
  bottomCfg.deltaZMin = m_deltaZMin;
  bottomCfg.deltaZMax = m_deltaZMax;
  bottomCfg.impactMax = m_impactMax;
  bottomCfg.interactionPointCut = true;
  bottomCfg.collisionRegionMin = -1e9f;
  bottomCfg.collisionRegionMax = +1e9f;
  bottomCfg.cotThetaMax = m_cotThetaMax;
  bottomCfg.minPt = m_minPt;
  bottomCfg.helixCutTolerance = m_helixCutTolerance;

  auto bottomFinder = Acts::DoubletSeedFinder::create(
      Acts::DoubletSeedFinder::DerivedConfig(bottomCfg, m_bFieldInZ));

  Acts::DoubletSeedFinder::Config topCfg = bottomCfg;
  topCfg.candidateDirection = Acts::Direction::Forward();
  auto topFinder = Acts::DoubletSeedFinder::create(
      Acts::DoubletSeedFinder::DerivedConfig(topCfg, m_bFieldInZ));

  Acts::TripletSeedFinder::Config tripletCfg;
  tripletCfg.useStripInfo = false;
  tripletCfg.sortedByCotTheta = true;
  tripletCfg.minPt = m_minPt;
  tripletCfg.sigmaScattering = 1e9f;
  tripletCfg.radLengthPerSeed = 0.0f;
  tripletCfg.impactMax = m_impactMax;
  tripletCfg.helixCutTolerance = m_helixCutTolerance;
  tripletCfg.toleranceParam = m_toleranceParam;

  auto tripletFinder = Acts::TripletSeedFinder::create(
      Acts::TripletSeedFinder::DerivedConfig(tripletCfg, m_bFieldInZ));

  Acts::BroadTripletSeedFilter::State filterState;
  Acts::BroadTripletSeedFilter::Cache filterCache;
  auto filterLogger = m_logger->cloneWithSuffix("Filter");
  Acts::BroadTripletSeedFilter seedFilter(m_filterCfg, filterState, filterCache,
                                          *filterLogger);

  auto finderLogger = m_logger->cloneWithSuffix("Finder");
  Acts::TripletSeeder seeder(std::move(finderLogger));

  static thread_local Acts::TripletSeeder::Cache cache;
  static thread_local Acts::Experimental::CylindricalSpacePointKDTree::Candidates candidates;

  for (const auto& middle : kdTree) {
    const auto spM = coreSpacePoints.at(middle.second);

    const float rM = spM.zr()[1];
    if (m_useVariableMiddleSPRange) {
      if (rM < m_rMinMiddle || rM > m_rMaxMiddle) {
        continue;
      }
    }

    const float zM = spM.zr()[0];
    if (zM < m_zMin || zM > m_zMax) {
      continue;
    }
    if (const float phiM = spM.phi(); phiM < m_phiMin || phiM > m_phiMax) {
      continue;
    }

    candidates.clear();
    kdTree.validTuples(lhOptions, hlOptions, spM, 0u, candidates);

    auto bottomSps = coreSpacePoints.subset(candidates.bottom_lh_v);
    auto topSps = coreSpacePoints.subset(candidates.top_lh_v);

    auto doGroup = [&](Acts::SpacePointContainer2::ConstSubset& btm,
                       Acts::SpacePointContainer2::ConstSubset& top) {  
      const auto before = outSeeds.size();

      seeder.createSeedsFromGroup(cache, *bottomFinder, *topFinder,
                                  *tripletFinder, seedFilter, coreSpacePoints,
                                  btm, spM, top, outSeeds);

      if (!m_enableStraightLineCut) {
        return;
      }

      for (std::size_t i = before; i < outSeeds.size(); ++i) {
        auto idx = outSeeds[i].spacePointIndices();

        auto s0 = coreSpacePoints.at(idx[0]);
        auto s1 = coreSpacePoints.at(idx[1]);
        auto s2 = coreSpacePoints.at(idx[2]);

        Acts::Vector3 p0(s0.xy()[0], s0.xy()[1], s0.zr()[0]);
        Acts::Vector3 p1(s1.xy()[0], s1.xy()[1], s1.zr()[0]);
        Acts::Vector3 p2(s2.xy()[0], s2.xy()[1], s2.zr()[0]);

        const float c = collinearity(p0, p1, p2);
        if (c > m_maxCollinearity) {
          outSeeds[i].quality() = -1.0f;
        }
      }
    };

    doGroup(bottomSps, topSps);

    bottomSps = coreSpacePoints.subset(candidates.bottom_hl_v);
    topSps = coreSpacePoints.subset(candidates.top_hl_v);
    doGroup(bottomSps, topSps);
  }

  if (m_enableStraightLineCut) {
    Acts::SeedContainer2 compact;
    compact.reserve(outSeeds.size(), 3.0f);

    for (std::size_t i = 0; i < outSeeds.size(); ++i) {
      const auto sold = outSeeds[i];
      if (sold.quality() < 0.0f) {
        continue;
      }

      auto snew = compact.createSeed();
      snew.assignSpacePointIndices(sold.spacePointIndices());
      snew.quality() = sold.quality();
      snew.vertexZ() = sold.vertexZ();
    }

    outSeeds = std::move(compact);
  }

  info() << "SeedingTool produced " << outSeeds.size() << " seeds from "
         << coreSpacePoints.size() << " space points" << endmsg;

  return StatusCode::SUCCESS;
}
