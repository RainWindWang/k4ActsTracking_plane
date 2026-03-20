#include "DigitizationSvc.h"
#include <GaudiKernel/IRndmGenSvc.h>
#include <fmt/format.h>

DECLARE_COMPONENT(DigitizationSvc)

DigitizationSvc::DigitizationSvc(const std::string& name, ISvcLocator* svcLoc)
    : base_class(name, svcLoc) {}

StatusCode DigitizationSvc::initialize() {
  StatusCode sc = Service::initialize();
  if (!sc.isSuccess()) {
    return sc;
  }

  m_rndmSvc = service<IRndmGenSvc>(m_rndmSvcName.value()).get();
  if (!m_rndmSvc) {
    error() << "DigitizationSvc: failed to retrieve IRndmGenSvc='"
            << m_rndmSvcName.value() << "'" << endmsg;
    return StatusCode::FAILURE;
  }

  try {
    m_decoder = std::make_unique<dd4hep::DDSegmentation::BitFieldCoder>(m_idLayout.value());

    // detect x/y presence
    try {
      (void)m_decoder->get(0ULL, "x");
      m_hasX = true;
    } catch (...) {
      m_hasX = false;
    }
    try {
      (void)m_decoder->get(0ULL, "y");
      m_hasY = true;
    } catch (...) {
      m_hasY = false;
    }

    info() << fmt::format("DigitizationSvc: BitFieldCoder initialized. hasX={}, hasY={}, layout='{}'",
                          m_hasX, m_hasY, m_idLayout.value())
           << endmsg;
  } catch (const std::exception& e) {
    error() << fmt::format("DigitizationSvc: failed to construct BitFieldCoder from layout '{}': {}",
                           m_idLayout.value(), e.what())
            << endmsg;
    return StatusCode::FAILURE;
  }

  return StatusCode::SUCCESS;
}

StatusCode DigitizationSvc::finalize() {
  std::scoped_lock lock{m_mutex};
  m_decoder.reset();
  m_rndmSvc = nullptr;
  return Service::finalize();
}

std::optional<int> DigitizationSvc::getField(std::uint64_t cellID, const char* name) const {
  std::scoped_lock lock{m_mutex};
  if (!m_decoder) {
    return std::nullopt;
  }
  try {
    return static_cast<int>(m_decoder->get(cellID, name));
  } catch (...) {
    return std::nullopt;
  }
}

IDigitizationSvc::CellFields DigitizationSvc::decode(std::uint64_t cellID) const {
  CellFields f;
  if (auto v = getField(cellID, "system"); v) f.system = *v;
  if (auto v = getField(cellID, "side"); v) f.side = *v;
  if (auto v = getField(cellID, "layer"); v) f.layer = *v;
  if (auto v = getField(cellID, "module"); v) f.module = *v;
  if (auto v = getField(cellID, "sensor"); v) f.sensor = *v;

  if (m_hasX) f.x = getField(cellID, "x");
  if (m_hasY) f.y = getField(cellID, "y");
  return f;
}

std::optional<int> DigitizationSvc::decodeX(std::uint64_t cellID) const {
  if (!m_hasX) return std::nullopt;
  return getField(cellID, "x");
}

std::optional<int> DigitizationSvc::decodeY(std::uint64_t cellID) const {
  if (!m_hasY) return std::nullopt;
  return getField(cellID, "y");
}

double DigitizationSvc::varU() const {
  const double sigma = m_pitchX.value() / std::sqrt(12.0);
  return sigma * sigma;
}

double DigitizationSvc::varV() const {
  const double sigma = m_pitchY.value() / std::sqrt(12.0);
  return sigma * sigma;
}
