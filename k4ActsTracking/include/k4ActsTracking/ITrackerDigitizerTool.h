#pragma once

#include <GaudiKernel/IAlgTool.h>
#include <GaudiKernel/EventContext.h>

#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/TrackerHitPlaneCollection.h>

class GAUDI_API ITrackerDigitizerTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ITrackerDigitizerTool, 1, 0);

  virtual StatusCode digitize(const EventContext& ctx,
                              const edm4hep::SimTrackerHitCollection& in,
                              edm4hep::TrackerHitPlaneCollection& out) const = 0;

  ~ITrackerDigitizerTool() override = default;
};
