#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <k4FWCore/DataHandle.h>

class TrackFittingTool;

class TrackFittingAlg final : public Gaudi::Algorithm {
public:
  TrackFittingAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_inTracks{
      "ActsTracks", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr> m_inTrackStates{
      "ActsTrackStates", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_outTracks{
      "FittedActsTracks", Gaudi::DataHandle::Writer, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr> m_outTrackStates{
      "FittedActsTrackStates", Gaudi::DataHandle::Writer, this};

  ToolHandle<TrackFittingTool> m_tool{
      this, "Tool", "TrackFittingTool/TrackFittingTool"};
};
