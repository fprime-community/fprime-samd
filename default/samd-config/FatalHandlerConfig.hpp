// ======================================================================
// \title  FatalHandlerConfig.hpp
// \brief  Assertion packet repeat count and spacing of Samd21::FatalHandler
// ======================================================================
#ifndef SAMD21_FATAL_HANDLER_CFG_HPP_
#define SAMD21_FATAL_HANDLER_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace FatalHandlerConfig {

//! Times the assertion packet is sent out of fatalOut before the MCU is reset
static constexpr FwIndexType PACKET_REPEAT = 10;

//! Busy-wait between repeated assertion packets (ms)
static constexpr U32 PACKET_DELAY_MS = 50;

}  // namespace FatalHandlerConfig
}  // namespace Samd21

#endif  // SAMD21_FATAL_HANDLER_CFG_HPP_
