#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/IAlgTool.h>

#include <edm4hep/TrackCollection.h>

class IActsToEdm4hepTrackWriterTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(IActsToEdm4hepTrackWriterTool, 1, 0);

  /// Convert ACTS tracks into edm4hep track collection
  virtual StatusCode writeTracks(
      const k4ActsTracking::ActsTrackContainerPtr& tracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& trackStates,
      edm4hep::TrackCollection& outTracks) const = 0;

  ~IActsToEdm4hepTrackWriterTool() override = default;
};
