// ======================================================================
// \title  ProcTimeTester.cpp
// \author tumbar
// \brief  cpp file for ProcTime test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/ProcTime/test/ut/ProcTimeTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "Os/RawTime.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ProcTimeTester::ProcTimeTester()
    : ProcTimeGTestBase("ProcTimeTester", ProcTimeTester::MAX_HISTORY_SIZE), component("ProcTime") {
    this->initComponents();
    this->connectPorts();
}

ProcTimeTester::~ProcTimeTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

Fw::Time ProcTimeTester::rawTimeNow() {
    Os::RawTime now;
    EXPECT_EQ(now.now(), Os::RawTimeInterface::Status::OP_OK);

    // A default-constructed RawTime is the clock's zero point
    const Os::RawTime epoch;
    Fw::TimeInterval elapsed;
    EXPECT_EQ(now.getTimeInterval(epoch, elapsed), Os::RawTimeInterface::Status::OP_OK);
    return Fw::Time(TimeBase::TB_PROC_TIME, elapsed.getSeconds(), elapsed.getUSeconds());
}

Fw::Time ProcTimeTester::getTime() {
    Fw::Time time;
    this->invoke_to_getTime(0, time);
    return time;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ProcTimeTester::testTimeBase() {
    REQUIREMENT("SAMD21-PROCTIME-001: ProcTime shall report time in the TB_PROC_TIME time base");

    const Fw::Time time = this->getTime();
    ASSERT_EQ(time.getTimeBase(), TimeBase::TB_PROC_TIME);
    ASSERT_EQ(time.getContext(), 0);
}

void ProcTimeTester::testTimeTracksRawTime() {
    REQUIREMENT("SAMD21-PROCTIME-002: ProcTime shall report the time elapsed on the Os::RawTime clock");

    // The reported time must fall between raw time readings taken either side of the call
    const Fw::Time before = rawTimeNow();
    const Fw::Time time = this->getTime();
    const Fw::Time after = rawTimeNow();

    ASSERT_LE(before, time);
    ASSERT_LE(time, after);
}

void ProcTimeTester::testTimeIsMonotonic() {
    REQUIREMENT("SAMD21-PROCTIME-002: ProcTime shall report the time elapsed on the Os::RawTime clock");

    Fw::Time previous = this->getTime();
    for (U32 i = 0; i < 100; i++) {
        const Fw::Time time = this->getTime();
        ASSERT_LE(previous, time);
        previous = time;
    }
}

}  // namespace Samd21
