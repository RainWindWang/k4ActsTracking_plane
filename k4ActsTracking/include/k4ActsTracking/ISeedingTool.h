#pragma once

#include <GaudiKernel/IAlgTool.h>

#include <Acts/EventData/SeedContainer2.hpp>
#include <Acts/EventData/SpacePointContainer2.hpp>

class ISeedingTool : virtual public IAlgTool {
public:
  DeclareInterfaceID(ISeedingTool, 1, 0);

  /// Build ACTS seeds from an ACTS SpacePointContainer2.
  ///
  /// NOTE:
  ///   The input SpacePointContainer2 must already have the columns required by
  ///   the implementation enabled (at least XY|ZR|Phi, and any additional ones
  ///   accessed internally).
  virtual StatusCode createSeeds(const Acts::SpacePointContainer2& spacePoints,
                                 Acts::SeedContainer2& outSeeds) const = 0;

  ~ISeedingTool() override = default;
};
