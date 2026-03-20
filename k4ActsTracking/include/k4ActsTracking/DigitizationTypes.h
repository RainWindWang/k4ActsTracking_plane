#pragma once

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/EventData/SpacePointContainer2.hpp>
#include <Acts/EventData/SourceLink.hpp>

#include <cstdint>
#include <vector>

namespace k4ActsTracking {

struct ActsSpacePointContainer2Wrapper {
  static constexpr Acts::SpacePointColumns kColumns =
      Acts::SpacePointColumns::SourceLinks |
      Acts::SpacePointColumns::XY |
      Acts::SpacePointColumns::ZR |
      Acts::SpacePointColumns::Phi |
      Acts::SpacePointColumns::VarianceZ |
      Acts::SpacePointColumns::VarianceR;

  Acts::SpacePointContainer2 sps;

  ActsSpacePointContainer2Wrapper() : sps(kColumns) {}
};

struct IndexSourceLink {
  Acts::GeometryIdentifier geoId;
  std::uint32_t hitIndex{0};
};

using SpacePointCollection = ActsSpacePointContainer2Wrapper;
using SourceLinkCollection = std::vector<IndexSourceLink>;

}  // namespace k4ActsTracking
