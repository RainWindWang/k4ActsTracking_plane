#include "SeedingTool.h"

#include "k4ActsTracking/DigitizationTypes.h"

#include <Acts/Definitions/Direction.hpp>
#include <Acts/Seeding2/CylindricalSpacePointKDTree.hpp>
#include <Acts/Seeding2/TripletSeeder.hpp>
#include <Acts/Utilities/Helpers.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

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

static inline bool isStrictMonotonic3(int a, int b, int c) {
  return ((a < b) && (b < c)) || ((a > b) && (b > c));
}

static inline bool isContinuousMonotonic3(int a, int b, int c) {
  return ((a + 1 == b) && (b + 1 == c)) || ((a - 1 == b) && (b - 1 == c));
}

struct SeedLineFit {
  bool valid = false;
  float ax = 0.0f;  // x = ax * z + bx
  float bx = 0.0f;
  float ay = 0.0f;  // y = ay * z + by
  float by = 0.0f;
};

struct SeedEval {
  bool valid = false;

  std::array<Acts::SpacePointIndex2, 3> spIdx = {0, 0, 0};
  std::array<int, 3> layers = {-1, -1, -1};

  bool uniqueLayers = false;
  bool monotonicLayers = false;
  bool continuousLayers = false;

  float col = 1e9f;
  SeedLineFit line;
  float tripletAvgResidual = 1e9f;
  float tripletMaxResidual = 1e9f;

  int supportLayers = 0;
  int supportPoints = 0;
  float supportAvgResidual = 1e9f;
  float supportMaxResidual = 1e9f;

  float score = -1.0f;
};

struct FamilyKey {
  int64_t axQ = 0;
  int64_t bxQ = 0;
  int64_t ayQ = 0;
  int64_t byQ = 0;

  bool operator==(const FamilyKey& other) const {
    return axQ == other.axQ && bxQ == other.bxQ &&
           ayQ == other.ayQ && byQ == other.byQ;
  }
};

struct FamilyKeyHash {
  std::size_t operator()(const FamilyKey& k) const {
    std::size_t h = 0;
    auto mix = [&](int64_t v) {
      std::size_t x = std::hash<int64_t>{}(v);
      h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    };
    mix(k.axQ);
    mix(k.bxQ);
    mix(k.ayQ);
    mix(k.byQ);
    return h;
  }
};

static inline int64_t quantizeToBin(float value, float bin) {
  return static_cast<int64_t>(std::llround(value / bin));
}

static inline float pointResidualToLine(const SeedLineFit& fit,
                                        float x, float y, float z) {
  const float xPred = fit.ax * z + fit.bx;
  const float yPred = fit.ay * z + fit.by;
  const float dx = x - xPred;
  const float dy = y - yPred;
  return std::sqrt(dx * dx + dy * dy);
}

static inline SeedLineFit fitLineXYZvsZ(const std::array<Acts::Vector3, 3>& pts) {
  SeedLineFit fit;

  const float z0 = static_cast<float>(pts[0].z());
  const float z1 = static_cast<float>(pts[1].z());
  const float z2 = static_cast<float>(pts[2].z());

  const float x0 = static_cast<float>(pts[0].x());
  const float x1 = static_cast<float>(pts[1].x());
  const float x2 = static_cast<float>(pts[2].x());

  const float y0 = static_cast<float>(pts[0].y());
  const float y1 = static_cast<float>(pts[1].y());
  const float y2 = static_cast<float>(pts[2].y());

  const float zMean = (z0 + z1 + z2) / 3.0f;
  const float xMean = (x0 + x1 + x2) / 3.0f;
  const float yMean = (y0 + y1 + y2) / 3.0f;

  const float szz =
      (z0 - zMean) * (z0 - zMean) +
      (z1 - zMean) * (z1 - zMean) +
      (z2 - zMean) * (z2 - zMean);

  if (std::abs(szz) < 1e-12f) {
    fit.valid = false;
    return fit;
  }

  const float sxz =
      (z0 - zMean) * (x0 - xMean) +
      (z1 - zMean) * (x1 - xMean) +
      (z2 - zMean) * (x2 - xMean);

  const float syz =
      (z0 - zMean) * (y0 - yMean) +
      (z1 - zMean) * (y1 - yMean) +
      (z2 - zMean) * (y2 - yMean);

  fit.ax = sxz / szz;
  fit.bx = xMean - fit.ax * zMean;
  fit.ay = syz / szz;
  fit.by = yMean - fit.ay * zMean;
  fit.valid = true;
  return fit;
}

static inline float computeTripletAvgResidual(const SeedLineFit& fit,
                                              const std::array<Acts::Vector3, 3>& pts,
                                              float& maxResidual) {
  float sum = 0.0f;
  maxResidual = 0.0f;

  for (const auto& p : pts) {
    const float r = pointResidualToLine(
        fit,
        static_cast<float>(p.x()),
        static_cast<float>(p.y()),
        static_cast<float>(p.z()));
    sum += r;
    maxResidual = std::max(maxResidual, r);
  }
  return sum / 3.0f;
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
    const auto& sl = slRange[0].get<k4ActsTracking::IndexSourceLink>();
    slInfo = fmt::format("hitIndex={}, geoId={}", sl.hitIndex, sl.geoId.value());
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

template <typename MsgStreamT>
void dumpSeedEval(MsgStreamT& log,
                  const SeedEval& e,
                  std::size_t seedIndex,
                  const std::string& prefix = "") {
  log << fmt::format(
      "{}seedEval[{}]: valid={} layers=({}, {}, {}) unique={} monotonic={} continuous={} "
      "col={:.8g} tripletAvgRes={:.6g} tripletMaxRes={:.6g} "
      "supportLayers={} supportPoints={} supportAvgRes={:.6g} supportMaxRes={:.6g} "
      "line(x=az+b: a={:.8g}, b={:.6g}; y=cz+d: c={:.8g}, d={:.6g}) "
      "score={:.6g}",
      prefix, seedIndex,
      e.valid ? 1 : 0,
      e.layers[0], e.layers[1], e.layers[2],
      e.uniqueLayers ? 1 : 0,
      e.monotonicLayers ? 1 : 0,
      e.continuousLayers ? 1 : 0,
      e.col,
      e.tripletAvgResidual, e.tripletMaxResidual,
      e.supportLayers, e.supportPoints,
      e.supportAvgResidual, e.supportMaxResidual,
      e.line.ax, e.line.bx, e.line.ay, e.line.by,
      e.score)
      << endmsg;
}

static inline bool betterSeedEval(const SeedEval& a,
                                  const SeedEval& b,
                                  bool preferContinuousTriplets) {
  constexpr float kResidualEps = 1.0e-6f;

  if (a.supportLayers != b.supportLayers) {
    return a.supportLayers > b.supportLayers;
  }
  if (a.supportPoints != b.supportPoints) {
    return a.supportPoints > b.supportPoints;
  }
  if (preferContinuousTriplets && a.continuousLayers != b.continuousLayers) {
    return a.continuousLayers && !b.continuousLayers;
  }
  if (std::abs(a.supportAvgResidual - b.supportAvgResidual) > kResidualEps) {
    return a.supportAvgResidual < b.supportAvgResidual;
  }
  if (std::abs(a.tripletAvgResidual - b.tripletAvgResidual) > kResidualEps) {
    return a.tripletAvgResidual < b.tripletAvgResidual;
  }
  if (std::abs(a.col - b.col) > 1e-12f) {
    return a.col < b.col;
  }
  return a.score > b.score;
}

static inline SeedEval evaluateSeed(const Acts::SpacePointContainer2& coreSpacePoints,
                                    const std::array<Acts::SpacePointIndex2, 3>& idx,
                                    float supportResidualMax,
                                    float supportLayerScoreWeight,
                                    float supportPointScoreWeight,
                                    float continuousTripletBonus,
                                    float supportResidualPenalty,
                                    float tripletResidualPenalty,
                                    float collinearityPenalty,
                                    bool preferContinuousTriplets) {
  SeedEval e;
  e.spIdx = idx;

  const auto s0 = coreSpacePoints.at(idx[0]);
  const auto s1 = coreSpacePoints.at(idx[1]);
  const auto s2 = coreSpacePoints.at(idx[2]);

  const Acts::Vector3 p0(s0.xy()[0], s0.xy()[1], s0.zr()[0]);
  const Acts::Vector3 p1(s1.xy()[0], s1.xy()[1], s1.zr()[0]);
  const Acts::Vector3 p2(s2.xy()[0], s2.xy()[1], s2.zr()[0]);

  const std::array<Acts::Vector3, 3> pts = {p0, p1, p2};

  e.layers = {
      approxLayerFromZ(s0.zr()[0]),
      approxLayerFromZ(s1.zr()[0]),
      approxLayerFromZ(s2.zr()[0])
  };

  e.uniqueLayers =
      (e.layers[0] != e.layers[1]) &&
      (e.layers[0] != e.layers[2]) &&
      (e.layers[1] != e.layers[2]);

  e.monotonicLayers = isStrictMonotonic3(e.layers[0], e.layers[1], e.layers[2]);
  e.continuousLayers = isContinuousMonotonic3(e.layers[0], e.layers[1], e.layers[2]);

  e.col = collinearity(p0, p1, p2);

  e.line = fitLineXYZvsZ(pts);
  if (!e.line.valid) {
    e.valid = false;
    return e;
  }

  e.tripletAvgResidual = computeTripletAvgResidual(e.line, pts, e.tripletMaxResidual);

  std::array<bool, 4> layerUsed = {false, false, false, false};
  for (int l : e.layers) {
    if (l >= 0 && l < 4) {
      layerUsed[l] = true;
    }
  }

  float supportResidualSum = 0.0f;
  e.supportMaxResidual = 0.0f;

  for (int extraLayer = 0; extraLayer < 4; ++extraLayer) {
    if (layerUsed[extraLayer]) {
      continue;
    }

    float bestResThisLayer = std::numeric_limits<float>::max();
    bool found = false;

    for (Acts::SpacePointIndex2 isp = 0; isp < coreSpacePoints.size(); ++isp) {
      if (isp == idx[0] || isp == idx[1] || isp == idx[2]) {
        continue;
      }

      const auto sp = coreSpacePoints.at(isp);
      const int layer = approxLayerFromZ(sp.zr()[0]);
      if (layer != extraLayer) {
        continue;
      }

      const float res = pointResidualToLine(
          e.line, sp.xy()[0], sp.xy()[1], sp.zr()[0]);

      if (res < bestResThisLayer) {
        bestResThisLayer = res;
      }

      if (res < supportResidualMax) {
        found = true;
      }
    }

    if (found) {
      ++e.supportLayers;
      ++e.supportPoints;
      supportResidualSum += bestResThisLayer;
      e.supportMaxResidual = std::max(e.supportMaxResidual, bestResThisLayer);
    }
  }

  if (e.supportPoints > 0) {
    e.supportAvgResidual = supportResidualSum / static_cast<float>(e.supportPoints);
  } else {
    e.supportAvgResidual = 1e9f;
    e.supportMaxResidual = 1e9f;
  }

  e.score =
      supportLayerScoreWeight * static_cast<float>(e.supportLayers) +
      supportPointScoreWeight * static_cast<float>(e.supportPoints) +
      ((preferContinuousTriplets && e.continuousLayers) ? continuousTripletBonus : 0.0f) -
      supportResidualPenalty * e.supportAvgResidual -
      tripletResidualPenalty * e.tripletAvgResidual -
      collinearityPenalty * e.col;

  e.valid = true;
  return e;
}

static inline FamilyKey makeFamilyKey(const SeedEval& e,
                                      float familySlopeBin,
                                      float familyInterceptBin) {
  FamilyKey key;
  key.axQ = quantizeToBin(e.line.ax, familySlopeBin);
  key.bxQ = quantizeToBin(e.line.bx, familyInterceptBin);
  key.ayQ = quantizeToBin(e.line.ay, familySlopeBin);
  key.byQ = quantizeToBin(e.line.by, familyInterceptBin);
  return key;
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
         << ", enableLayerOrderCut=" << (m_enableLayerOrderCut ? "ON" : "OFF")
         << ", preferContinuousTriplets=" << (m_preferContinuousTriplets ? "ON" : "OFF")
         << ", tripletResidualMax=" << m_tripletResidualMax
         << ", supportResidualMax=" << m_supportResidualMax
         << ", enableFamilyDuplicateSuppression="
         << (m_enableFamilyDuplicateSuppression ? "ON" : "OFF")
         << ", familySlopeBin=" << m_familySlopeBin
         << ", familyInterceptBin=" << m_familyInterceptBin
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

  std::vector<SeedEval> seedEvals;

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

      if (seedEvals.size() < afterCreate) {
        seedEvals.resize(afterCreate);
      }

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

      for (std::size_t i = before; i < afterCreate; ++i) {
        auto idx = outSeeds[i].spacePointIndices();

        const std::array<Acts::SpacePointIndex2, 3> idxArr = {
            idx[0], idx[1], idx[2]
        };

        SeedEval eval = evaluateSeed(coreSpacePoints, idxArr,
                                     m_supportResidualMax.value(),
                                     m_supportLayerScoreWeight.value(),
                                     m_supportPointScoreWeight.value(),
                                     m_continuousTripletBonus.value(),
                                     m_supportResidualPenalty.value(),
                                     m_tripletResidualPenalty.value(),
                                     m_collinearityPenalty.value(),
                                     m_preferContinuousTriplets.value());
        seedEvals[i] = eval;

        debug() << fmt::format(
            "    seed[{}] in group[{}]: collinearity={:.8g}",
            i, label, eval.col)
                << endmsg;
        dumpSeedEval(debug(), eval, i, "      ");

        // straight-line / collinearity cut
        if (eval.col > m_maxCollinearity.value()) {
          debug() << fmt::format(
              "    reject seed[{}] in group[{}]: collinearity={:.8g} > max={:.8g}",
              i, label, eval.col, m_maxCollinearity.value())
                  << endmsg;
          dumpSeed(debug(), coreSpacePoints, outSeeds[i], i, "      reject ");
          outSeeds[i].quality() = -1.0f;
          continue;
        }

        // layer uniqueness + monotonic layer order
        if (m_enableLayerOrderCut &&
            (!eval.uniqueLayers || !eval.monotonicLayers)) {
          debug() << fmt::format(
              "    reject seed[{}] in group[{}]: uniqueLayers={} monotonicLayers={} "
              "layers=({}, {}, {})",
              i, label,
              eval.uniqueLayers ? 1 : 0,
              eval.monotonicLayers ? 1 : 0,
              eval.layers[0], eval.layers[1], eval.layers[2])
                  << endmsg;
          dumpSeed(debug(), coreSpacePoints, outSeeds[i], i, "      reject ");
          outSeeds[i].quality() = -1.0f;
          continue;
        }

        // fitted-line residual sanity
        if (!eval.line.valid || eval.tripletAvgResidual > m_tripletResidualMax.value()) {
          debug() << fmt::format(
              "    reject seed[{}] in group[{}]: invalid line fit or tripletAvgResidual={:.6g} > max={:.6g}",
              i, label, eval.tripletAvgResidual, m_tripletResidualMax.value())
                  << endmsg;
          dumpSeed(debug(), coreSpacePoints, outSeeds[i], i, "      reject ");
          outSeeds[i].quality() = -1.0f;
          continue;
        }

        // 4th-layer support scoring -> seed quality
        outSeeds[i].quality() = eval.score;

        debug() << fmt::format(
            "    keep seed[{}] in group[{}]: score={:.6g}, supportLayers={}, supportPoints={}, "
            "tripletAvgResidual={:.6g}, supportAvgResidual={:.6g}, continuousLayers={}",
            i, label,
            eval.score, eval.supportLayers, eval.supportPoints,
            eval.tripletAvgResidual, eval.supportAvgResidual,
            eval.continuousLayers ? 1 : 0)
                << endmsg;
      }
    };

    doGroup(bottomSps, topSps, "lh");

    bottomSps = coreSpacePoints.subset(candidates.bottom_hl_v);
    topSps = coreSpacePoints.subset(candidates.top_hl_v);
    doGroup(bottomSps, topSps, "hl");
  }

  if (m_enableStraightLineCut) {
    // seed family duplicate suppression
    if (m_enableFamilyDuplicateSuppression) {
      std::unordered_map<FamilyKey, std::size_t, FamilyKeyHash> bestSeedPerFamily;

      for (std::size_t i = 0; i < outSeeds.size(); ++i) {
        if (outSeeds[i].quality() < 0.0f) {
          continue;
        }
        if (i >= seedEvals.size()) {
          continue;
        }

        const auto& eval = seedEvals[i];
        if (!eval.valid || !eval.line.valid) {
          outSeeds[i].quality() = -1.0f;
          continue;
        }

        const FamilyKey key = makeFamilyKey(
            eval, m_familySlopeBin.value(), m_familyInterceptBin.value());

        auto it = bestSeedPerFamily.find(key);
        if (it == bestSeedPerFamily.end()) {
          bestSeedPerFamily.emplace(key, i);
          debug() << fmt::format(
              "SeedingTool: family create for seed[{}] key=({}, {}, {}, {})",
              i, key.axQ, key.bxQ, key.ayQ, key.byQ)
                  << endmsg;
        } else {
          const std::size_t oldIdx = it->second;
          const auto& oldEval = seedEvals[oldIdx];

          if (betterSeedEval(eval, oldEval, m_preferContinuousTriplets.value())) {
            debug() << fmt::format(
                "SeedingTool: family replace seed[{}] -> seed[{}] "
                "(supportLayers {}->{}, supportPoints {}->{}, tripletAvgRes {:.6g}->{:.6g}, "
                "supportAvgRes {:.6g}->{:.6g}, continuous {}->{})",
                oldIdx, i,
                oldEval.supportLayers, eval.supportLayers,
                oldEval.supportPoints, eval.supportPoints,
                oldEval.tripletAvgResidual, eval.tripletAvgResidual,
                oldEval.supportAvgResidual, eval.supportAvgResidual,
                oldEval.continuousLayers ? 1 : 0,
                eval.continuousLayers ? 1 : 0)
                    << endmsg;
            outSeeds[oldIdx].quality() = -1.0f;
            it->second = i;
          } else {
            debug() << fmt::format(
                "SeedingTool: family reject duplicate seed[{}], keep seed[{}]",
                i, oldIdx)
                    << endmsg;
            outSeeds[i].quality() = -1.0f;
          }
        }
      }
    }

    // compact surviving seeds
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
