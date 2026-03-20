#pragma once

#include "k4ActsTracking/ITrackerDigitizerTool.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>
#include <Gaudi/Property.h>

#include <k4FWCore/DataHandle.h>

#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/TrackerHitPlaneCollection.h>

class DigiAlg final : public Gaudi::Algorithm {
public:
  DigiAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  Gaudi::Property<std::string> m_inputName{this, "InputCollection", "SiHits", "Input SimTrackerHit collection"};
  Gaudi::Property<std::string> m_outputName{this, "OutputCollection", "DigiTrackerHits", "Output TrackerHitPlane collection"};

  mutable k4FWCore::DataHandle<edm4hep::SimTrackerHitCollection> m_inHits{
      m_inputName, Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<edm4hep::TrackerHitPlaneCollection> m_outHits{
      m_outputName, Gaudi::DataHandle::Writer, this};

  ToolHandleArray<ITrackerDigitizerTool> m_tools{this, "Tools", {}, "Digitizer tools (tracker)"};
};
