#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/AlgTool.h>

#include <Acts/Utilities/Logger.hpp>

#include <edm4hep/TrackCollection.h>
#include <edm4hep/TrackStateCollection.h>

#include <memory>

class ActsToEdm4hepTrackWriterTool final
    : public extends<GaudiTool, IAlgTool> {
public:
  ActsToEdm4hepTrackWriterTool(const std::string& type,
                               const std::string& name,
                               const IInterface* parent);

  StatusCode initialize() override;

  StatusCode write(
      const k4ActsTracking::ActsTrackContainerPtr& inTracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& inTrackStates,
      edm4hep::TrackCollection& outTracks,
      edm4hep::TrackStateCollection& outTrackStates) const;

private:
  std::unique_ptr<const Acts::Logger> m_logger;
};
