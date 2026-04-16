#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/DigitizationTypes.h"
#include "k4ActsTracking/ITrackFindingTool.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <k4FWCore/DataHandle.h>

class TrackFindingCKFAlg final : public Gaudi::Algorithm {
public:
  TrackFindingCKFAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  mutable k4FWCore::DataHandle<k4ActsTracking::InitialTrackParametersCollection>
      m_inInitialTrackParameters{"TrackerInitialTrackParameters",
                                 Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::SourceLinkCollection> m_inSourceLinks{
      "TrackerSourceLinks", Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection> m_inMeasurements{
      "TrackerMeasurements", Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::ActsTrackContainerPtr> m_outTracks{
      "ActsTracks", Gaudi::DataHandle::Writer, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::ActsTrackStateContainerPtr>
      m_outTrackStates{"ActsTrackStates", Gaudi::DataHandle::Writer, this};

  mutable ToolHandle<ITrackFindingTool> m_tool{
      this, "Tool", "TrackFindingCKFTool/TrackFindingCKFTool"};
};
