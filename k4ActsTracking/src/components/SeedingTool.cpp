#include "SeedingTool.h"

#include "k4ActsTracking/DigitizationTypes.h"

#include <Acts/Definitions/Direction.hpp>
#include <Acts/Seeding2/CylindricalSpacePointKDTree.hpp>
#include <Acts/Seeding2/TripletSeeder.hpp>
#include <Acts/Utilities/Helpers.hpp>

#include <fmt/format.h>

#include <cmath>
#include <limits>
#include <string>

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

static inline int approxLayerFromZ(float z) {
  // Debug helper for current LUXE test geometry
  constexpr float zLayers[] = {3961.9f, 4061.9f, 4161.9f, 4261.9f};

  int bestLayer = -1;
  float bestDz = std::numeric_limits<float>::max();
  for (int i = 0; i < 4; ++i) {
    const float dz = std::abs(z - zLayers[i]);
    if (dz < bestDz) {
      bestDz = dz;
      bestLayer = i;
    }
  }
  return bestLayer;
}

template <typename MsgStreamT>
void dumpSpacePoint(MsgStreamT& log,
                    const Acts::SpacePointContainer2& sps,
                    Acts::SpacePointIndex2 idx,
                    const std::string& prefix = "") {
  const auto sp = sps.at(idx);

  const auto xy = sp.xy();
  const auto zr = sp.zr();

  const float x = xy[0];
  const float y = xy[1];
  const float z = zr[0];
  const float r = zr[1];
  const float phi = sp.phi();
  const float varR = sp.varianceR();
  const float varZ = sp.varianceZ();
  const int layer = approxLayerFromZ(z);

std::string slInfo = "nSL=0";
const auto slRange = sp.sourceLinks();
if (!slRange.empty()) {
  try {
    const auto* sl = slRange[0].get<const k4ActsTracking::IndexSourceLink*>();
    if (sl != nullptr) {
      slInfo = fmt::format("hitIndex={}, geoId={}", sl->hitIndex, sl->geoId.value());
    } else {
      slInfo = "SL=nullptr";
    }
  } catch (const std::bad_any_cast&) {
    slInfo = "SL=bad_any_cast";
  }
}

  log << fmt::format(
      "{}SP[{:<2}] layer~{} x={:9.3f} y={:9.3f} z={:9.3f} r={:9.3f} "
      "phi={:8.5f} varR={:.6g} varZ={:.6g} {}",
      prefix, idx, layer, x, y, z, r, phi, varR, varZ, slInfo)
      << endmsg;
}

template <typename MsgStreamT, typename SeedProxyT>
void dumpSeed(MsgStreamT& log,
              const Acts::SpacePointContainer2& sps,
              const SeedProxyT& seed,
              std::size_t seedIndex,
              const std::string& prefix = "") {
  const auto idx = seed.spacePointIndices();

  log << fmt::format(
      "{}Seed[{:<2}] quality={:.6g} vertexZ={:.6g} spIdx=({}, {}, {})",
      prefix, seedIndex, seed.quality(), seed.vertexZ(),
      idx[0], idx[1], idx[2])
      << endmsg;

  dumpSpacePoint(log, sps, idx[0], prefix + "  -> ");
  dumpSpacePoint(log, sps, idx[1], prefix + "  -> ");
  dumpSpacePoint(log, sps, idx[2], prefix + "  -> ");
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
         << ", maxCollinearity=" << m_maxCollinearity
	 << endmsg;

  return StatusCode::SUCCESS;
}

StatusCode SeedingTool::createSeeds(
    const Acts::SpacePointContainer2& coreSpacePoints,
    Acts::SeedContainer2& outSeeds) const {
  outSeeds.clear();

  info() << fmt::format("SeedingTool: begin createSeeds with {} space points",
                        coreSpacePoints.size())
         << endmsg;

  for (Acts::SpacePointIndex2 i = 0; i < coreSpacePoints.size(); ++i) {
    dumpSpacePoint(debug(), coreSpacePoints, i, "SeedingTool: input ");
  }

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
    const auto middleIdx = middle.second;
    const auto spM = coreSpacePoints.at(middleIdx);

    const float rM = spM.zr()[1];
    if (m_useVariableMiddleSPRange) {
      if (rM < m_rMinMiddle || rM > m_rMaxMiddle) {
        debug() << fmt::format(
            "SeedingTool: skip middle SP[{}] by middle-r cut, r={:.3f}",
            middleIdx, rM)
                << endmsg;
        continue;
      }
    }

    const float zM = spM.zr()[0];
    if (zM < m_zMin || zM > m_zMax) {
      debug() << fmt::format(
          "SeedingTool: skip middle SP[{}] by z cut, z={:.3f}",
          middleIdx, zM)
              << endmsg;
      continue;
    }
    if (const float phiM = spM.phi(); phiM < m_phiMin || phiM > m_phiMax) {
      debug() << fmt::format(
          "SeedingTool: skip middle SP[{}] by phi cut, phi={:.5f}",
          middleIdx, phiM)
              << endmsg;
      continue;
    }

    debug() << fmt::format("SeedingTool: middle SP[{}]", middleIdx) << endmsg;
    dumpSpacePoint(debug(), coreSpacePoints, middleIdx, "  ");

    candidates.clear();
    kdTree.validTuples(lhOptions, hlOptions, spM, 0u, candidates);

    debug() << fmt::format(
        "  candidates for middle SP[{}]: bottom_lh={} top_lh={} bottom_hl={} top_hl={}",
        middleIdx,
        candidates.bottom_lh_v.size(), candidates.top_lh_v.size(),
        candidates.bottom_hl_v.size(), candidates.top_hl_v.size())
            << endmsg;

    auto bottomSps = coreSpacePoints.subset(candidates.bottom_lh_v);
    auto topSps = coreSpacePoints.subset(candidates.top_lh_v);

    auto doGroup = [&](Acts::SpacePointContainer2::ConstSubset& btm,
                       Acts::SpacePointContainer2::ConstSubset& top,
                       const char* label) {
      const auto before = outSeeds.size();

      debug() << fmt::format(
          "  doGroup[{}]: middleSP={} bottomSubset={} topSubset={}",
          label, middleIdx, btm.size(), top.size())
              << endmsg;

      seeder.createSeedsFromGroup(cache, *bottomFinder, *topFinder,
                                  *tripletFinder, seedFilter, coreSpacePoints,
                                  btm, spM, top, outSeeds);

      const auto afterCreate = outSeeds.size();

      debug() << fmt::format(
          "  doGroup[{}]: created {} raw seed(s)",
          label, afterCreate - before)
              << endmsg;

      for (std::size_t i = before; i < afterCreate; ++i) {
        dumpSeed(debug(), coreSpacePoints, outSeeds[i], i, "    raw ");
      }

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

        debug() << fmt::format(
            "    straightLineCut seed[{}] in group[{}]: collinearity={:.8g}",
            i, label, c)
                << endmsg;

        if (c > m_maxCollinearity.value()) {
          debug() << fmt::format(
              "    reject seed[{}] in group[{}]: collinearity={:.8g} > max={:.8g}",
              i, label, c, m_maxCollinearity.value())
                  << endmsg;
          dumpSeed(debug(), coreSpacePoints, outSeeds[i], i, "      reject ");
          outSeeds[i].quality() = -1.0f;
        }
      }
    };

    doGroup(bottomSps, topSps, "lh");

    bottomSps = coreSpacePoints.subset(candidates.bottom_hl_v);
    topSps = coreSpacePoints.subset(candidates.top_hl_v);
    doGroup(bottomSps, topSps, "hl");
  }

  if (m_enableStraightLineCut) {
    Acts::SeedContainer2 compact;
    compact.reserve(outSeeds.size(), 3.0f);

    for (std::size_t i = 0; i < outSeeds.size(); ++i) {
      const auto sold = outSeeds[i];
      if (sold.quality() < 0.0f) {
        debug() << fmt::format("SeedingTool: compact drops seed[{}]", i) << endmsg;
        continue;
      }

      auto snew = compact.createSeed();
      snew.assignSpacePointIndices(sold.spacePointIndices());
      snew.quality() = sold.quality();
      snew.vertexZ() = sold.vertexZ();
    }

    outSeeds = std::move(compact);
  }

  info() << fmt::format("SeedingTool: final surviving seeds = {} from {} space points",
                        outSeeds.size(), coreSpacePoints.size())
         << endmsg;

  for (std::size_t i = 0; i < outSeeds.size(); ++i) {
    dumpSeed(info(), coreSpacePoints, outSeeds[i], i, "  final ");
  }

  info() << "SeedingTool produced " << outSeeds.size() << " seeds from "
         << coreSpacePoints.size() << " space points" << endmsg;

  return StatusCode::SUCCESS;
}

