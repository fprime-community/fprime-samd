// ======================================================================
// \title  PassiveRateGroupDriver.hpp
// \author tumbar
// \brief  hpp file for PassiveRateGroupDriver component implementation class
// ======================================================================

#ifndef Samd21_PassiveRateGroupDriver_HPP
#define Samd21_PassiveRateGroupDriver_HPP

#include "fprime-samd/Svc/PassiveRateGroupDriver/PassiveRateGroupDriverComponentAc.hpp"

namespace Samd21 {

class PassiveRateGroupDriver final : public PassiveRateGroupDriverComponentBase {
  public:
    //! Size of the divider table, provided as a constants to users passing the table in
    static const FwIndexType DIVIDER_SIZE = NUM_CYCLEOUT_OUTPUT_PORTS;

    //! \class Divider
    //! \brief Struct describing a divider
    struct Divider {
        //! Initializes divisor and offset to 0 (unused)
        constexpr Divider() : divisor(0), offset(0) {}
        //! Initializes divisor and offset to passed-in pair
        constexpr Divider(FwSizeType divisorIn, FwSizeType offsetIn) : divisor(divisorIn), offset(offsetIn) {}
        //! Divisor
        FwSizeType divisor;
        //! Offset
        FwSizeType offset;
    };

    //! \class DividerSet
    //! \brief Struct containing an array of dividers
    struct DividerSet {
        //! Dividers
        Divider dividers[Samd21::PassiveRateGroupDriver::DIVIDER_SIZE];
    };

    void configure(const DividerSet& dividerSet);

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PassiveRateGroupDriver object
    PassiveRateGroupDriver(const char* const compName  //!< The component name
    );

    //! Destroy PassiveRateGroupDriver object
    ~PassiveRateGroupDriver();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for CycleIn
    //!
    //! Cycle input to the rate group driver
    void CycleIn_handler(FwIndexType portNum,     //!< The port number
                         Os::RawTime& cycleStart  //!< Cycle start timestamp
                         ) override;

    //! divider array
    Divider m_dividers[NUM_CYCLEOUT_OUTPUT_PORTS];

    //! tick counter
    FwSizeType m_ticks;

    //! rollover counter
    FwSizeType m_rollover;

    //!< maximum execution time in microseconds
    U32 m_maxTime;

    //! has the configure method been called
    bool m_configured;
};

}  // namespace Samd21

#endif
