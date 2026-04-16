#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/IAlgTool.h>

#include <optional>

class ITrackParamsEstimationTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackParamsEstimationTool, 1, 0);

  /// Estimate initial bound track parameters for ONE seed.
  /// Return std::nullopt if the seed cannot be converted into a valid initial
  /// parameter estimate.
  virtual std::optional<Acts::BoundTrackParameters> estimateOneSeed(
      const k4ActsTracking::SpacePointContainer& spacePoints,
      const Acts::SeedContainer2::ConstProxy& seed,
      const k4ActsTracking::MeasurementCollection& measurements) const = 0;

  ~ITrackParamsEstimationTool() override = default;
};
