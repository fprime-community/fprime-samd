// ======================================================================
// \title  PassiveRateGroupDriver.cpp
// \author tumbar
// \brief  cpp file for PassiveRateGroupDriver component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveRateGroupDriver/PassiveRateGroupDriver.hpp"
#include "Utils/Algorithms.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PassiveRateGroupDriver ::PassiveRateGroupDriver(const char* const compName)
    : PassiveRateGroupDriverComponentBase(compName), m_ticks(0), m_rollover(1), m_maxTime(0), m_configured(false) {}

PassiveRateGroupDriver ::~PassiveRateGroupDriver() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PassiveRateGroupDriver::configure(const DividerSet& dividerSet) {
    // check arguments
    FW_ASSERT(dividerSet.dividers);
    // verify port/table size matches
    FW_ASSERT(FW_NUM_ARRAY_ELEMENTS(this->m_dividers) == this->getNum_CycleOut_OutputPorts(),
              static_cast<FwAssertArgType>(FW_NUM_ARRAY_ELEMENTS(this->m_dividers)),
              static_cast<FwAssertArgType>(this->getNum_CycleOut_OutputPorts()));
    // restart the tick count so a re-configure does not accumulate into the rollover
    this->m_ticks = 0;
    this->m_rollover = 1;
    // copy provided array of dividers
    for (FwIndexType entry = 0; entry < PassiveRateGroupDriver::DIVIDER_SIZE; entry++) {
        // A port with an offset equal or bigger than the divisor is not accepted because it would never be called
        FW_ASSERT((dividerSet.dividers[entry].offset == 0) ||
                      (dividerSet.dividers[entry].offset < dividerSet.dividers[entry].divisor),
                  static_cast<FwAssertArgType>(dividerSet.dividers[entry].offset),
                  static_cast<FwAssertArgType>(dividerSet.dividers[entry].divisor));
        this->m_dividers[entry] = dividerSet.dividers[entry];
        // Rollover value should be the least common multiple of all dividers to make sure integer rollover doesn't
        // jump cycles. Only use non-zero dividers. Utils::lcm asserts if the result would overflow.
        if (dividerSet.dividers[entry].divisor != 0) {
            this->m_rollover = Utils::lcm(this->m_rollover, dividerSet.dividers[entry].divisor);
        }
    }
    this->m_configured = true;
}

void PassiveRateGroupDriver ::CycleIn_handler(FwIndexType portNum, Os::RawTime& cycleStart) {
    // Make sure that the dividers have been configured:
    // If this asserts, add the configure() call to initialization.
    FW_ASSERT(this->m_configured);

    Os::RawTime endTime;

    // Loop through each divider. For a given port, the port will be called when the divider value
    // divides evenly into the number of ticks. For example, if the divider value for a port is 4,
    // it would be called every fourth invocation of the CycleIn port.
    for (FwIndexType entry = 0; entry < PassiveRateGroupDriver::DIVIDER_SIZE; entry++) {
        if (this->m_dividers[entry].divisor != 0) {
            if (this->isConnected_CycleOut_OutputPort(static_cast<FwIndexType>(entry))) {
                if ((this->m_ticks % this->m_dividers[entry].divisor) == this->m_dividers[entry].offset) {
                    // Grab the start of the cycle
                    Os::RawTime this_cycle;
                    this_cycle.now();

                    this->CycleOut_out(static_cast<FwIndexType>(entry), this_cycle);
                }
            }
        }
    }

    // rollover the tick value when the tick count reaches the rollover value
    // the rollover value is the least common multiple of all the dividers, see configure()
    FW_ASSERT(this->m_rollover > 0);
    this->m_ticks = (this->m_ticks + 1) % this->m_rollover;

    // grab timer for endTime of cycle
    endTime.now();

    // get rate group execution time
    U32 cycleTime;
    // Cast to void as the only possible error is overflow, which we can't handle other
    // than capping cycleTime to max value of U32 (which is done in getDiffUsec anyways)
    (void)endTime.getDiffUsec(cycleStart, cycleTime);
    // check to see if the time has exceeded the previous maximum
    if (cycleTime > this->m_maxTime) {
        this->m_maxTime = cycleTime;
    }

    this->tlmWrite_MaxCycleTime(this->m_maxTime);
    this->tlmWrite_CycleTime(cycleTime);
}

}  // namespace Samd21
