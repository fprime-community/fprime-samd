// ======================================================================
// \title  Samd21Time.cpp
// \author tumbar
// \brief  cpp file for Samd21Time component implementation class
// ======================================================================

#include "fprime-samd/Svc/Samd21Time/Samd21Time.hpp"
#include "Fw/Time/Time.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/OsSelection.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Samd21Time ::Samd21Time(const char* const compName) : Samd21TimeComponentBase(compName) {}

Samd21Time ::~Samd21Time() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Samd21Time ::getTime_handler(FwIndexType portNum, Fw::Time& time) {
    Os::RawTime now;
    auto status = now.now();
    FW_ASSERT(status == Os::RawTimeInterface::Status::OP_OK, status);

    const Os::RawTime ref_time;

    Fw::TimeInterval proc_time;
    status = now.getTimeInterval(ref_time, proc_time);
    FW_ASSERT(status == Os::RawTimeInterface::Status::OP_OK, status);

    time = Fw::Time(TimeBase::TB_PROC_TIME, proc_time.getSeconds(), proc_time.getUSeconds());
}


}  // namespace Samd21
