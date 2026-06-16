// ======================================================================
// \title Samd21/Os/Mutex/DefaultMutex.cpp
// \brief sets default Os::Mutex and ConditionVariable implementation via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "fprime-samd/Os/Mutex.hpp"
#include "Os/Stub/ConditionVariable.hpp"

namespace Os {

//! \brief get a delegate for MutexInterface for Samd21
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
MutexInterface* MutexInterface::getDelegate(MutexHandleStorage& aligned_new_memory) {
    static_assert(sizeof(Os::Samd21::Mutex) <= sizeof(MutexHandleStorage), "Mutex storage is too small");
    return Os::Delegate::makeDelegate<MutexInterface, Os::Samd21::Mutex>(aligned_new_memory);
}

//! \brief get a delegate for ConditionVariableInterface using Stub implementation
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
ConditionVariableInterface* ConditionVariableInterface::getDelegate(
    ConditionVariableHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<ConditionVariableInterface, Os::Stub::Mutex::StubConditionVariable,
                                      ConditionVariableHandleStorage>(aligned_new_memory);
}

}  // namespace Os
