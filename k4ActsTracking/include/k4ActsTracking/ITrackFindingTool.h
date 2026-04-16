#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/DigitizationTypes.h"

#include <GaudiKernel/IAlgTool.h>

class ITrackFindingTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackFindingTool, 1, 0);

  /// Build ACTS track candidates from:
  ///   - initial track parameter estimates
  ///   - source links
  ///   - measurements
  virtual StatusCode findTracks(
      const k4ActsTracking::InitialTrackParametersCollection& initialParameters,
      const k4ActsTracking::SourceLinkCollection& sourceLinks,
      const k4ActsTracking::MeasurementCollection& measurements,
      k4ActsTracking::ActsTrackContainerPtr& outTracks,
      k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const = 0;

  ~ITrackFindingTool() override = default;
};
