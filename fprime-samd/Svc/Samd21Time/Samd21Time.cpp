// ======================================================================
// \title  Samd21Time.cpp
// \author tumbar
// \brief  cpp file for Samd21Time component implementation class
// ======================================================================

#include "fprime-samd/Svc/Samd21Time/Samd21Time.hpp"
#include "Fw/Time/Time.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/Assert.hpp"
#include "Os/RawTime.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Samd21Time ::Samd21Time(const char* const compName) : Samd21TimeComponentBase(compName), m_proc_ref_time() {}

Samd21Time ::~Samd21Time() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Samd21Time ::getTime_handler(FwIndexType portNum, Fw::Time& time) {
    Os::RawTime now;
    auto status = now.now();
    FW_ASSERT(status == Os::RawTimeInterface::Status::OP_OK, status);

    Fw::TimeInterval proc_time;
    status = now.getTimeInterval(m_proc_ref_time, proc_time);
    FW_ASSERT(status == Os::RawTimeInterface::Status::OP_OK, status);

    // TODO(tumbar) Use the SC time rather than the PROC time when configured
    time = Fw::Time(TimeBase::TB_PROC_TIME, proc_time.getSeconds(), proc_time.getUSeconds());
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Samd21Time ::SET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::TimeValue value) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Samd21
