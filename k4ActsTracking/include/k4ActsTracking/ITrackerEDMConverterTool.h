#pragma once

#include "k4ActsTracking/DigitizationTypes.h"
#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/IAlgTool.h>
#include <GaudiKernel/EventContext.h>

#include <edm4hep/TrackerHitPlaneCollection.h>

class GAUDI_API ITrackerEDMConverterTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackerEDMConverterTool, 1, 0);

  virtual StatusCode convert(const EventContext& ctx,
                             const edm4hep::TrackerHitPlaneCollection& inHits,
                             k4ActsTracking::SpacePointCollection& outSP,
                             k4ActsTracking::SourceLinkCollection& outSL,
                             k4ActsTracking::MeasurementProvider& outMeas) const = 0;

  ~ITrackerEDMConverterTool() override = default;
};
