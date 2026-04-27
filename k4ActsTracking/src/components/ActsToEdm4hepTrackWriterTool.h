#pragma once

#include "k4ActsTracking/IActsToEdm4hepTrackWriterTool.h"

#include <Gaudi/Property.h>
#include <GaudiKernel/AlgTool.h>

#include <Acts/Utilities/Logger.hpp>

#include <memory>

class ActsToEdm4hepTrackWriterTool final
    : public extends<AlgTool, IActsToEdm4hepTrackWriterTool> {
public:
  ActsToEdm4hepTrackWriterTool(const std::string& type,
                               const std::string& name,
                               const IInterface* parent);

  StatusCode initialize() override;

  StatusCode writeTracks(
      const k4ActsTracking::ActsTrackContainerPtr& tracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& trackStates,
      edm4hep::TrackCollection& outTracks) const override;

private:
  Gaudi::Property<bool> m_dumpSummary{
      this, "DumpSummary", true,
      "Print summary of ACTS to edm4hep track conversion"};

  std::unique_ptr<const Acts::Logger> m_logger;
};
