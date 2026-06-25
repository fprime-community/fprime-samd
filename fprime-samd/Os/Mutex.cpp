// ======================================================================
// \title Samd21/Os/Mutex/Mutex.cpp
// \brief stub implementation for Samd21Os::Mutex
// ======================================================================
#include "Mutex.hpp"
#include <sam.h>
#include "Os/Mutex.hpp"

namespace Os {
namespace Samd21 {

Os::MutexInterface::Status Mutex::take() {
    // Spin-lock: keep trying until we acquire the mutex
    U32 spin_n = 0;
    while (spin_n < 100) {
        __disable_irq();
        if (!this->m_handle.m_mutex_taken) {
            // Mutex is available, acquire it
            this->m_handle.m_mutex_taken = true;
            __enable_irq();
            break;
        }
        __enable_irq();

        spin_n++;
    }

    if (spin_n >= 100) {
        return Os::MutexInterface::Status::ERROR_BUSY;
    }

    return Os::MutexInterface::Status::OP_OK;
}

Os::MutexInterface::Status Mutex::release() {
    // Attempt to mark the mutex as not taken.
    if (!this->m_handle.m_mutex_taken) {
        // The mutex was already not taken, which indicates a coding defect.
        return Os::MutexInterface::Status::ERROR_OTHER;
    }

    this->m_handle.m_mutex_taken = false;

    // The mutex was taken.
    // Now that it has been marked as not taken, we have successfully exited the critical section.
    return Os::MutexInterface::Status::OP_OK;
}

Os::MutexHandle* Mutex::getHandle() {
    return &this->m_handle;
}
}  // namespace Samd21
}  // namespace Os
