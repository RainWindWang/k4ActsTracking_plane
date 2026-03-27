#pragma once

#include "k4ActsTracking/DigitizationTypes.h"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/EventData/SeedContainer2.hpp>
#include <Acts/EventData/SpacePointContainer2.hpp>
#include <Acts/EventData/TrackParameters.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Surfaces/Surface.hpp>
#include <Acts/TrackFitting/TrackContainer.hpp>
#include <Acts/EventData/MultiTrajectory.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace k4ActsTracking {

struct TrackerMeasurement2D {
  Acts::Vector2 loc{0, 0};
  Acts::SquareMatrix2 cov = Acts::SquareMatrix2::Zero();
  Acts::GeometryIdentifier geoId;
  const Acts::Surface* surface{nullptr};
  std::uint32_t hitIndex{0};
};

using MeasurementCollection = std::vector<TrackerMeasurement2D>;

using SpacePointContainer = Acts::SpacePointContainer2;
using SpacePointCollection = SpacePointContainer;

using SeedContainer = Acts::SeedContainer2;

using InitialTrackParametersCollection =
    std::vector<Acts::BoundTrackParameters>;

using ProtoTrack = std::vector<std::uint32_t>;
using ProtoTrackCollection = std::vector<ProtoTrack>;

using ActsTrackContainer = Acts::VectorTrackContainer;
using ActsTrackStateContainer = Acts::VectorMultiTrajectory;

using ActsTrackContainerPtr = std::shared_ptr<ActsTrackContainer>;
using ActsTrackStateContainerPtr =
    std::shared_ptr<ActsTrackStateContainer>;

}  // namespace k4ActsTracking
