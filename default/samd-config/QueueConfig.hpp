// ======================================================================
// \title  QueueConfig.hpp
// \brief  Configuration of the SAMD21 Os::Queue implementation
// ======================================================================
#ifndef SAMD21_QUEUE_CFG_HPP_
#define SAMD21_QUEUE_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace QueueConfig {

//! Bytes of static memory shared by every Os::Queue. Each queue consumes
//! depth * (messageSize + sizeof(FwSizeType)) plus alignment padding.
//! Queue creation fails (asserting in the component init) when the topology's queues do not fit.
static constexpr FwSizeType ARENA_SIZE = 1024;

}  // namespace QueueConfig
}  // namespace Samd21

#endif  // SAMD21_QUEUE_CFG_HPP_
