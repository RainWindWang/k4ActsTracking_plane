#pragma once

#include "k4ActsTracking/ActsDataContainers.h"
#include "k4ActsTracking/DigitizationTypes.h"
#include "k4ActsTracking/IActsGeoSvc.h"
#include "k4ActsTracking/ITrackFindingTool.h"

#include <Gaudi/Property.h>
#include <GaudiKernel/AlgTool.h>
#include <GaudiKernel/ServiceHandle.h>

#include <Acts/EventData/SourceLink.hpp>
#include <Acts/EventData/TrackContainer.hpp>
#include <Acts/EventData/VectorMultiTrajectory.hpp>
#include <Acts/TrackFinding/MeasurementSelector.hpp>
#include <Acts/Utilities/Logger.hpp>

#include <memory>
#include <utility>
#include <vector>

class TrackFindingCKFTool final : public extends<AlgTool, ITrackFindingTool> {
public:
  TrackFindingCKFTool(const std::string& type, const std::string& name,
                      const IInterface* parent);

  StatusCode initialize() override;

  StatusCode findTracks(
      const k4ActsTracking::InitialTrackParametersCollection& initialParameters,
      const k4ActsTracking::SourceLinkCollection& sourceLinks,
      const k4ActsTracking::MeasurementCollection& measurements,
      k4ActsTracking::ActsTrackContainerPtr& outTracks,
      k4ActsTracking::ActsTrackStateContainerPtr& outTrackStates) const override;

private:
  ServiceHandle<IActsGeoSvc> m_geoSvc{this, "ActsGeoSvc", "ActsGeoSvc",
                                      "ACTS geometry / field service"};

  Gaudi::Property<double> m_chi2CutOff{
      this, "Chi2CutOff", 15.0,
      "Maximum local chi2 contribution for measurement selection"};

  Gaudi::Property<unsigned int> m_numMeasurementsCutOff{
      this, "NumMeasurementsCutOff", 10u,
      "Maximum number of associated measurements on a single surface"};

  Gaudi::Property<unsigned int> m_maxSteps{
      this, "MaxSteps", 10000u, "Maximum number of propagation steps"};

  Gaudi::Property<bool> m_reverseSearch{
      this, "ReverseSearch", false,
      "Run CKF in reverse direction and target a perigee surface"};

  Gaudi::Property<bool> m_resolvePassive{
      this, "ResolvePassive", false, "Navigator resolve passive surfaces"};

  Gaudi::Property<bool> m_resolveMaterial{
      this, "ResolveMaterial", true, "Navigator resolve material surfaces"};

  Gaudi::Property<bool> m_resolveSensitive{
      this, "ResolveSensitive", true, "Navigator resolve sensitive surfaces"};

  Gaudi::Property<bool> m_trimTracks{
      this, "TrimTracks", true,
      "Trim trailing / leading states before finalizing tracks"};

  Gaudi::Property<bool> m_dumpInitialParameters{
      this, "DumpInitialParameters", true,
      "Print a summary for each input initial parameter"};

  Gaudi::Property<bool> m_dumpTrackSummary{
      this, "DumpTrackSummary", true,
      "Print a summary for each found CKF candidate track"};

  std::unique_ptr<const Acts::Logger> m_logger;
};
