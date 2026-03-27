#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/DigitizationTypes.h"
#include "k4ActsTracking/ITrackParamsEstimationTool.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>

#include <k4FWCore/DataHandle.h>

class TrackParamsEstimationAlg final : public Gaudi::Algorithm {
public:
  TrackParamsEstimationAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  k4FWCore::DataHandle<k4ActsTracking::SpacePointCollection> m_inSP{
      "TrackerSpacePoints", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::SeedContainer> m_inSeeds{
      "TrackerSeeds", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection> m_inMeas{
      "TrackerMeasurements", Gaudi::DataHandle::Reader, this};

  k4FWCore::DataHandle<k4ActsTracking::InitialTrackParametersCollection> m_outParams{
      "TrackerInitialTrackParameters", Gaudi::DataHandle::Writer, this};

  ToolHandle<ITrackParamsEstimationTool> m_tool{
      this, "Tool", "TrackParamsEstimationTool/TrackParamsEstimationTool"};
};
