#pragma once

#include "k4ActsTracking/DigitizationTypes.h"
#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/ITrackerEDMConverterTool.h"

#include <GaudiKernel/Algorithm.h>
#include <GaudiKernel/ToolHandle.h>
#include <Gaudi/Property.h>

#include <k4FWCore/DataHandle.h>

#include <edm4hep/TrackerHitPlaneCollection.h>

class TrackerEDMToActsAlg final : public Gaudi::Algorithm {
public:
  TrackerEDMToActsAlg(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode execute(const EventContext& ctx) const override;

private:
  Gaudi::Property<std::string> m_inHitsName{
      this, "InputHits", "DigiTrackerHits", "Input TrackerHitPlane collection"};
  Gaudi::Property<std::string> m_outSPName{
      this, "OutputSpacePoints", "TrackerSpacePoints", "Output space points"};
  Gaudi::Property<std::string> m_outSLName{
      this, "OutputSourceLinks", "TrackerSourceLinks", "Output source links"};
  Gaudi::Property<std::string> m_outMeasName{
      this, "OutputMeasurements", "TrackerMeasurements", "Output measurement provider"};

  mutable k4FWCore::DataHandle<edm4hep::TrackerHitPlaneCollection> m_inHits{
      m_inHitsName, Gaudi::DataHandle::Reader, this};

  mutable k4FWCore::DataHandle<k4ActsTracking::SpacePointCollection> m_outSP{
      m_outSPName, Gaudi::DataHandle::Writer, this};
  mutable k4FWCore::DataHandle<k4ActsTracking::SourceLinkCollection> m_outSL{
      m_outSLName, Gaudi::DataHandle::Writer, this};
  mutable k4FWCore::DataHandle<k4ActsTracking::MeasurementCollection> m_outMeas{
      m_outMeasName, Gaudi::DataHandle::Writer, this};

  ToolHandle<ITrackerEDMConverterTool> m_tool{
      this, "Tool", "TrackerEDMConverterTool", "EDM->ACTS converter tool"};
};
