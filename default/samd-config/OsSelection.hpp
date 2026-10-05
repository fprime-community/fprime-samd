// ======================================================================
// \title default/samd-config/OsDelegateMutex.hpp
// \brief compile-time selection of Os::Mutex/Os::ConditionVariable for the SAMD21 baremetal platform
//
// Baremetal SAMD21 deployments are single-threaded (no RTOS, no active components), so contention
// over a mutex is structurally impossible. This overrides Os::Mutex/Os::ConditionVariable to alias
// directly to Samd21::Mutex and the framework's no-op StubConditionVariable, skipping the link-time
// Os::DelegateMutex/Os::DelegateConditionVariable indirection (see fprime's own
// config/OsDelegateMutex.hpp for the general mechanism and its compile-time-selection example).
// ======================================================================
#ifndef CONFIG_OSSELECTION_HPP
#define CONFIG_OSSELECTION_HPP

namespace Os {
namespace Samd21 {
class Mutex;
class RawTime;
}  // namespace Samd21
namespace Stub {
namespace Mutex {
class StubConditionVariable;
}  // namespace Mutex
}  // namespace Stub
}  // namespace Os

//!< Forward declarations of the link-time delegates
//!< Default: select the implementations at link time
namespace Os {

using Mutex = Os::Samd21::Mutex;
using ConditionVariable = Os::Stub::Mutex::StubConditionVariable;
using RawTime = Os::Samd21::RawTime;

}  // namespace Os

#define OS_RAW_TIME_HEADER "fprime-samd/Drv/RtcDriver/RawTime.hpp"
#define OS_MUTEX_HEADER "fprime-samd/Os/Mutex.hpp"
#define OS_CONDITION_VARIABLE_HEADER "Os/Stub/ConditionVariable.hpp"

#endif  // CONFIG_OSSELECTION_HPP
