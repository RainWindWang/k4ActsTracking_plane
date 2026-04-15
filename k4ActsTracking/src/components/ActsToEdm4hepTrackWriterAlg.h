#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <edm4hep/TrackCollection.h>
#include <edm4hep/TrackStateCollection.h>

#include <k4FWCore/DataHandle.h>

class ActsToEdm4hepTrackWriterTool;

class ActsToEdm4hepTrackWriterAlg final : public Gaudi::Algorithm {
public:
  ActsToEdm4hepTrackWriterAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_inTracks{
      "FittedActsTracks", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr> m_inTrackStates{
      "FittedActsTrackStates", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<edm4hep::TrackCollection> m_outTracks{
      "ReconstructedTracks", Gaudi::DataHandle::Writer, this};

  k4FWCore::DataHandle<edm4hep::TrackStateCollection> m_outTrackStates{
      "ReconstructedTrackStates", Gaudi::DataHandle::Writer, this};

  ToolHandle<ActsToEdm4hepTrackWriterTool> m_tool{
      this, "Tool",
      "ActsToEdm4hepTrackWriterTool/ActsToEdm4hepTrackWriterTool"};
};
