#pragma once

#include <GaudiKernel/IService.h>

#include <cstdint>
#include <optional>
#include <string>

class IRndmGenSvc;

class GAUDI_API IDigitizationSvc : virtual public IService {
public:
  DeclareInterfaceID(IDigitizationSvc, 1, 0);
// LUXE: system:1,side:1,layer:3,module:5,sensor:0,x:32:-16,y:-16
  struct CellFields {
    int system{0};
    int side{0};
    int layer{0};
    int module{0};
    int sensor{0};
    std::optional<int> x;
    std::optional<int> y;
  };

  virtual IRndmGenSvc* rndmSvc() const = 0;

  virtual const std::string& idLayout() const = 0;

  virtual bool hasX() const = 0;
  virtual bool hasY() const = 0;

  virtual CellFields decode(std::uint64_t cellID) const = 0;
  virtual std::optional<int> decodeX(std::uint64_t cellID) const = 0;
  virtual std::optional<int> decodeY(std::uint64_t cellID) const = 0;

  virtual double pitchX() const = 0;   // [mm]
  virtual double pitchY() const = 0;   // [mm]
  virtual double offsetX() const = 0;  // [mm]
  virtual double offsetY() const = 0;  // [mm]

  virtual double varU() const = 0;     // [mm^2]
  virtual double varV() const = 0;     // [mm^2]

  // Gaussian smearing in u/v (sigma in mm) if needed
  virtual double smearSigmaU() const = 0;
  virtual double smearSigmaV() const = 0;

  virtual ~IDigitizationSvc() = default;
};
