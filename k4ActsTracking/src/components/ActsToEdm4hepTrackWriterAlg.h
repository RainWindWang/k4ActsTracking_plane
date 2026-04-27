#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/IActsToEdm4hepTrackWriterTool.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <edm4hep/TrackCollection.h>

#include <k4FWCore/DataHandle.h>

class ActsToEdm4hepTrackWriterAlg final : public Gaudi::Algorithm {
public:
  ActsToEdm4hepTrackWriterAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  Gaudi::Property<std::string> m_inTracksName{
      this, "InputTracks", "ActsTracks",
      "Input ACTS track container pointer"};

  Gaudi::Property<std::string> m_inTrackStatesName{
      this, "InputTrackStates", "ActsTrackStates",
      "Input ACTS track-state container pointer"};

  Gaudi::Property<std::string> m_outTracksName{
      this, "OutputTracks", "ReconstructedTracks",
      "Output edm4hep track collection"};

  mutable k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_inTracks{
      "ActsTracks", Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr>
      m_inTrackStates{"ActsTrackStates", Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<edm4hep::TrackCollection> m_outTracks{
      "ReconstructedTracks", Gaudi::DataHandle::Writer, this};

  mutable ToolHandle<IActsToEdm4hepTrackWriterTool> m_tool{
      this, "Tool",
      "ActsToEdm4hepTrackWriterTool/ActsToEdm4hepTrackWriterTool"};
};
