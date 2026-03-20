#pragma once

#include "SeedingTool.h"
#include "k4ActsTracking/DigitizationTypes.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>
#include <Gaudi/Property.h>

#include <k4FWCore/DataHandle.h>
#include <k4FWCore/GaudiChecks.h>

#include <Acts/EventData/SeedContainer2.hpp>

#include <cstdint>

class SeedingAlg final : public Gaudi::Algorithm {
public:
  SeedingAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  Gaudi::Property<std::string> m_inSpacePointsName{
      this, "InputSpacePoints", "TrackerSpacePoints",
      "Input SpacePoints (wrapper of Acts::SpacePointContainer2)"};
  Gaudi::Property<std::string> m_outSeedsName{
      this, "OutputSeeds", "TrackerSeeds", "Output Acts::SeedContainer2"};

  /// Input: produced by digitization/EDM->ACTS converter
  mutable k4FWCore::DataHandle<k4ActsTracking::SpacePointCollection> m_inSpacePoints{
      m_inSpacePointsName, Gaudi::DataHandle::Reader, this};

  /// Output seeds
  mutable k4FWCore::DataHandle<Acts::SeedContainer2> m_outSeeds{
      m_outSeedsName, Gaudi::DataHandle::Writer, this};

  ToolHandle<ISeedingTool> m_seedingTool{
      this, "SeedingTool", "SeedingTool/SeedingTool"};

  Gaudi::Property<bool> m_verbose{this, "Verbose", false, ""};
};
