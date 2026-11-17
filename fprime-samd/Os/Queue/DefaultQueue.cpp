// ======================================================================
// \title fprime-samd/Os/Queue/DefaultQueue.cpp
// \brief sets default Os::Queue to the SAMD21 statically allocated queue via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "Os/Queue.hpp"
#include "fprime-samd/Os/Queue/Queue.hpp"

namespace Os {
QueueInterface* QueueInterface::getDelegate(QueueHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<QueueInterface, Os::Samd21::Queue, QueueHandleStorage>(aligned_new_memory);
}
}  // namespace Os
