#pragma once

#include "k4ActsTracking/ActsDataContainers.h"

#include <GaudiKernel/AlgTool.h>

#include <Acts/Utilities/Logger.hpp>

#include <memory>

class TrackFittingTool final : public extends<GaudiTool, IAlgTool> {
public:
  TrackFittingTool(const std::string& type,
                   const std::string& name,
                   const IInterface* parent);

  StatusCode initialize() override;

  StatusCode fitTracks(
      const k4ActsTracking::ActsTrackContainerPtr& inTracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& inTrackStates,
      const k4ActsTracking::ActsTrackContainerPtr& outTracks,
      const k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const;

private:
  std::unique_ptr<const Acts::Logger> m_logger;
};
