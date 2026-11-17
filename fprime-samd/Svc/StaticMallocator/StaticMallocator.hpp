/**
 * \file
 * \author A. Tumbar
 * \brief A MemAllocator implementation class that hands out a static memory arena.
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
#include <cstddef>
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwEnumStoreTypeAliasAc.h"

namespace Samd21 {

//! \brief static memory arena handed out by bumping an offset
//!
//! Allocations are carved back to back from a single arena of SIZE bytes, so any number of differently sized users
//! can share one instance and waste nothing beyond alignment padding. Individual allocations are never recovered: the
//! arena resets only once every allocation has been deallocated. This fits users that allocate once at boot and never
//! free in flight, while still letting unit tests create and teardown.
template <FwSizeType SIZE>
class StaticMallocator : public Fw::MemAllocator {
  public:
    static_assert(SIZE > 0, "Arena size must be non-zero");

    StaticMallocator() = default;
    ~StaticMallocator() override = default;

    //! Allocate memory
    //!
    //! \param identifier the allocating entity identifier (not used)
    //! \param size the requested size, set to 0 when the arena cannot fit the request
    //! \param recoverable - flag to indicate the memory could be recoverable (always set to false)
    //! \param alignment - alignment requirement for the allocation, at most alignof(std::max_align_t)
    //! \return the pointer to memory, nullptr when the arena cannot fit the request
    void* allocate(const FwEnumStoreType identifier,
                   FwSizeType& size,
                   bool& recoverable,
                   FwSizeType alignment = alignof(std::max_align_t)) override {
        (void)identifier;
        FW_ASSERT((alignment > 0) && ((alignment & (alignment - 1)) == 0), static_cast<FwAssertArgType>(alignment));
        // `m_arena` is declared alignas(std::max_align_t), so the bound here is exactly the default argument above.
        // alignof(std::max_align_t) is 8 on the SAMD21 but 16 on Linux x86-64, keep the two expressions the same.
        FW_ASSERT(alignment <= alignof(std::max_align_t), static_cast<FwAssertArgType>(alignment));
        recoverable = false;

        // m_used <= SIZE so the round up cannot overflow
        const FwSizeType start = (this->m_used + alignment - 1) & ~(alignment - 1);
        if ((start > SIZE) || (size > (SIZE - start))) {
            size = 0;
            return nullptr;
        }
        this->m_used = start + size;
        this->m_allocations++;
        return &this->m_arena[start];
    }

    //! Deallocate memory
    //!
    //! The memory stays allocated until every allocation has been deallocated, which resets the arena.
    //!
    //! \param identifier the memory segment identifier (not used)
    //! \param ptr the pointer to memory returned by allocate()
    void deallocate(const FwEnumStoreType identifier, void* ptr) override {
        (void)identifier;
        FW_ASSERT((ptr >= static_cast<void*>(&this->m_arena[0])) && (ptr < static_cast<void*>(&this->m_arena[SIZE])));
        FW_ASSERT(this->m_allocations > 0);
        this->m_allocations--;
        if (this->m_allocations == 0) {
            this->m_used = 0;
        }
    }

    //! \return bytes of the arena consumed, including alignment padding
    FwSizeType getUsed() const { return this->m_used; }

  private:
    alignas(std::max_align_t) U8 m_arena[SIZE];  //!< Backing storage
    FwSizeType m_used = 0;                       //!< Offset of the first unallocated byte
    FwSizeType m_allocations = 0;                //!< Number of outstanding allocations
};

}  // namespace Samd21

#endif /* SAMD21_MALLOCALLOCATOR_HPP */
