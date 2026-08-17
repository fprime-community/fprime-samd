/**
 * \file
 * \author A. Tumbar
 * \brief A MemAllocator implementation class that uses static memory buckets.
 *
 * \copyright
 * Copyright 2026, by the California Institute of Technology.
 * ALL RIGHTS RESERVED.  United States Government Sponsorship
 * acknowledged.
 *
 */

#ifndef SAMD21_MALLOCALLOCATOR_HPP
#define SAMD21_MALLOCALLOCATOR_HPP

#include <Fw/Types/MemAllocator.hpp>
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwEnumStoreTypeAliasAc.h"

namespace Samd21 {

//! \brief static memory based memory allocator
//!
//! This class implements a memory allocator that uses static memory buckets.
template <size_t BUCKET_SIZE, FwEnumStoreType IDENT>
class StaticMallocator : public Fw::MemAllocator {
  public:
    static_assert(BUCKET_SIZE % 8 == 0, "Bucket size must a multiple of 8");

    StaticMallocator() = default;
    virtual ~StaticMallocator() = default;

    //! Allocate memory
    //!
    //! Allocate memory using malloc(). The identifier is unused and memory is never recoverable.
    //! malloc() guarantees alignment for any type and so does this allocator. It will not respect smaller alignments.
    //!
    //! \param identifier the allocating entity identifier (not used)
    //! \param size the requested size (not changed)
    //! \param recoverable - flag to indicate the memory could be recoverable (always set to false)
    //! \param alignment - alignment requirement for the allocation. Default: maximum alignment defined by C++.
    //! \return the pointer to memory. Zero if unable to allocate.
    void* allocate(const FwEnumStoreType identifier,
                   FwSizeType& size,
                   bool& recoverable,
                   FwSizeType alignment = alignof(std::max_align_t)) override {
        FW_ASSERT(identifier == IDENT, static_cast<FwAssertArgType>(identifier), static_cast<FwAssertArgType>(IDENT));
        FW_ASSERT(size <= BUCKET_SIZE, static_cast<FwAssertArgType>(size), static_cast<FwAssertArgType>(BUCKET_SIZE));
        FW_ASSERT(alignment <= 8, static_cast<FwAssertArgType>(alignment));
        FW_ASSERT(!used);

        recoverable = true;
        this->used = true;
        return reinterpret_cast<void*>(this->data);
    }
    //! Deallocate memory
    //!
    //! Deallocate memory previously allocated by allocate() using free(). The identifier is unused but should still
    //! match the original call.
    //!
    //! \param identifier the memory segment identifier (not used)
    //! \param ptr the pointer to memory returned by allocate()
    void deallocate(const FwEnumStoreType identifier, void* ptr) override {
        FW_ASSERT(identifier == IDENT, static_cast<FwAssertArgType>(identifier), static_cast<FwAssertArgType>(IDENT));
        FW_ASSERT(used);
        FW_ASSERT(ptr == reinterpret_cast<void*>(this->data));
        this->used = false;
    }

  private:
    // Use u64 so that we force 64-bit memory alignment
    U64 data[BUCKET_SIZE / 8];
    bool used;
};

}  // namespace Samd21

#endif /* SAMD21_MALLOCALLOCATOR_HPP */
