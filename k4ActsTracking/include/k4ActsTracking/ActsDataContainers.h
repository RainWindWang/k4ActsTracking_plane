#pragma once

#include "k4ActsTracking/DigitizationTypes.h"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Geometry/GeometryIdentifier.hpp>
#include <Acts/Surfaces/Surface.hpp>

#include <cstdint>
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

class MeasurementProvider {
public:
  MeasurementProvider() = default;

  explicit MeasurementProvider(MeasurementCollection m)
      : m_meas(std::move(m)) {}

  const TrackerMeasurement2D* find(std::uint32_t hitIndex) const {
    for (const auto& mm : m_meas) {
      if (mm.hitIndex == hitIndex) return &mm;
    }
    return nullptr;
  }

  const MeasurementCollection& measurements() const { return m_meas; }

private:
  MeasurementCollection m_meas;
};

}  // namespace k4ActsTracking
