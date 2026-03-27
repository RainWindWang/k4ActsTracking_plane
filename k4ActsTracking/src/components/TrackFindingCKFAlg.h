#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <k4FWCore/DataHandle.h>

class TrackFindingCKFTool;

class TrackFindingCKFAlg final : public Gaudi::Algorithm {
public:
  TrackFindingCKFAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  k4FWCore::DataHandle<k4ActsTracking::InitialTrackParametersCollection> m_inParams{
      "TrackerInitialTrackParameters", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection> m_inMeasurements{
      "TrackerMeasurements", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_outTracks{
      "ActsTracks", Gaudi::DataHandle::Writer, this};

  k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr> m_outTrackStates{
      "ActsTrackStates", Gaudi::DataHandle::Writer, this};

  ToolHandle<TrackFindingCKFTool> m_tool{
      this, "Tool", "TrackFindingCKFTool/TrackFindingCKFTool"};
};
