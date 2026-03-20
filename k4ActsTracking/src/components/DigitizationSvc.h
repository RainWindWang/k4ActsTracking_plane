#pragma once

#include "k4ActsTracking/IDigitizationSvc.h"

#include <GaudiKernel/Service.h>
#include <Gaudi/Property.h>

#include <DDSegmentation/BitFieldCoder.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>

class IRndmGenSvc;

class DigitizationSvc final : public extends<Service, IDigitizationSvc> {
public:
  DigitizationSvc(const std::string& name, ISvcLocator* svcLoc);

  StatusCode initialize() override;
  StatusCode finalize() override;

  IRndmGenSvc* rndmSvc() const override { return m_rndmSvc; }

  const std::string& idLayout() const override { return m_idLayout.value(); }

  bool hasX() const override { return m_hasX; }
  bool hasY() const override { return m_hasY; }

  CellFields decode(std::uint64_t cellID) const override;
  std::optional<int> decodeX(std::uint64_t cellID) const override;
  std::optional<int> decodeY(std::uint64_t cellID) const override;

  double pitchX() const override { return m_pitchX.value(); }
  double pitchY() const override { return m_pitchY.value(); }
  double offsetX() const override { return m_offsetX.value(); }
  double offsetY() const override { return m_offsetY.value(); }

  double varU() const override;
  double varV() const override;

  double smearSigmaU() const override { return m_smearSigmaU.value(); }
  double smearSigmaV() const override { return m_smearSigmaV.value(); }

private:
  std::optional<int> getField(std::uint64_t cellID, const char* name) const;

private:
  Gaudi::Property<std::string> m_rndmSvcName{
      this, "RndmGenSvc", "RndmGenSvc", "Random number service"};

  Gaudi::Property<std::string> m_idLayout{
      this,
      "IDLayout",
      "system:1,side:1,layer:2,module:1,sensor:5,x:32:-16,y:-16",
      "DD4hep BitFieldCoder layout string"};

  Gaudi::Property<double> m_pitchX{this, "PitchX", 0.1, "Pitch X (u) [mm]"};
  Gaudi::Property<double> m_pitchY{this, "PitchY", 0.1, "Pitch Y (v) [mm]"};
  Gaudi::Property<double> m_offsetX{this, "OffsetX", 0.0, "Offset X [mm]"};
  Gaudi::Property<double> m_offsetY{this, "OffsetY", 0.0, "Offset Y [mm]"};

  Gaudi::Property<double> m_smearSigmaU{this, "SmearSigmaU", 0.0, "Gaussian smearing sigma in u [mm] (0=off)"};
  Gaudi::Property<double> m_smearSigmaV{this, "SmearSigmaV", 0.0, "Gaussian smearing sigma in v [mm] (0=off)"};

  IRndmGenSvc* m_rndmSvc{nullptr};

  mutable std::mutex m_mutex{};
  std::unique_ptr<dd4hep::DDSegmentation::BitFieldCoder> m_decoder{};
  bool m_hasX{false};
  bool m_hasY{false};
};
