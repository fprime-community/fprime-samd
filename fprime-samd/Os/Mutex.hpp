// Copyright 2026 California Institute of Technology
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

// ======================================================================
// \title Samd21/Os/Mutex/Mutex.hpp
// \brief definitions for Samd21Os::Mutex
// ======================================================================

#include "Os/Mutex.hpp"

#ifndef Samd21Os_Os_Mutex_HPP
#define Samd21Os_Os_Mutex_HPP

namespace Os {
namespace Samd21 {

struct MutexHandle : public Os::MutexHandle {
    //! True if the mutex has been acquired without being released.
    volatile bool m_mutex_taken = {false};
};

//! \brief Nonblocking stub implementation of Os::Mutex
//!
//! Stub implementation of `MutexInterface` for use as a delegate class.
//!
//! This mutex will never block, which allows it to be used on any platform without OS dependencies.
//! It is unsuitable for use in environments where threads need to contend over mutexes.
//! However, it is appropriate for use in environments with multiple threads that are not intended to
//! contend over mutexes, and where contention would indicate a coding defect worthy of an assertion.
//!
class Mutex : public Os::MutexInterface {
  public:
    //! \brief constructor
    //!
    Mutex() = default;

    //! \brief destructor
    //!
    ~Mutex() override = default;

    //! \brief return the underlying mutex handle (implementation specific)
    //! \return internal mutex handle representation
    Os::MutexHandle* getHandle() override;

    Os::MutexInterface::Status take() override;     //!<  lock the mutex and get return status
    Os::MutexInterface::Status release() override;  //!<  unlock the mutex and get return status

  private:
    //! Handle for StubMutex
    MutexHandle m_handle;
};

}  // namespace Samd21
} // namespace Os
#endif  // Samd21Os_Os_Mutex_HPP