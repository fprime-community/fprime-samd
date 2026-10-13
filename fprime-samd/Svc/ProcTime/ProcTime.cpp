// ======================================================================
// \title  ProcTime.cpp
// \author tumbar
// \brief  cpp file for ProcTime component implementation class
// ======================================================================

#include "fprime-samd/Svc/ProcTime/ProcTime.hpp"
#include "Fw/Time/Time.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/Assert.hpp"
#include "Os/RawTime.hpp"
#include "config/OsSelection.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ProcTime::ProcTime(const char* const compName) : ProcTimeComponentBase(compName) {}

ProcTime::~ProcTime() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ProcTime::getTime_handler(FwIndexType portNum, Fw::Time& time) {
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
