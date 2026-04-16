#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/DigitizationTypes.h"

#include <GaudiKernel/IAlgTool.h>

class ITrackFittingTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackFittingTool, 1, 0);

  /// Fit ACTS tracks from:
  ///   - initial track parameter estimates
  ///   - proto tracks
  ///   - source links
  ///   - measurements
  virtual StatusCode fitTracks(
      const k4ActsTracking::InitialTrackParametersCollection& initialParameters,
      const k4ActsTracking::ProtoTrackCollection& protoTracks,
      const k4ActsTracking::SourceLinkCollection& sourceLinks,
      const k4ActsTracking::MeasurementCollection& measurements,
      k4ActsTracking::ActsTrackContainerPtr& outTracks,
      k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const = 0;

  ~ITrackFittingTool() override = default;
};
