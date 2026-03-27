#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/IAlgTool.h>

class ITrackParamsEstimationTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackParamsEstimationTool, 1, 0);

  /// Estimate initial parameters for ONE seed
  virtual StatusCode estimateOneSeed(
      const k4ActsTracking::SpacePointContainer& spacePoints,
      const Acts::SeedContainer2::ConstProxy& seed,
      const k4ActsTracking::MeasurementCollection& measurements,
      Acts::BoundTrackParameters& outParams) const = 0;

  ~ITrackParamsEstimationTool() override = default;
};
