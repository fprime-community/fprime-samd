// ======================================================================
// \title  CriticalSection.hpp
// \author tumbar
// \brief  hpp file for CriticalSection utility
// ======================================================================

#ifndef Samd21_CriticalSection_HPP
#define Samd21_CriticalSection_HPP

namespace Samd21 {

//! Nestable interrupt-disabling critical section for the SAMD21.
//!
//! Disables global interrupts on entry to the outermost section and restores
//! the previous interrupt state when the outermost section exits. Nested
//! enter()/leave() pairs are reference-counted, so an inner section will not
//! prematurely re-enable interrupts.
//!
//! Prefer the RAII form for scoped protection:
//! \code
//!   {
//!       Samd21::CriticalSection cs;  // interrupts disabled here
//!       // ... access shared-with-ISR state ...
//!   }                                // interrupts restored here
//! \endcode
class CriticalSection {
  public:
    //! Enter a critical section (disable interrupts if outermost)
    static void enter();

    //! Leave a critical section (restore interrupts if outermost)
    static void leave();

    //! RAII: enter on construction
    CriticalSection();

    //! RAII: leave on destruction
    ~CriticalSection();

    // Not copyable or movable (manages a global interrupt-state resource)
    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;
};

}  // namespace Samd21

#endif
