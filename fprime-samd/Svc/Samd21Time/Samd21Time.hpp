// ======================================================================
// \title  Samd21Time.hpp
// \author tumbar
// \brief  hpp file for Samd21Time component implementation class
// ======================================================================

#ifndef Samd21_Samd21Time_HPP
#define Samd21_Samd21Time_HPP

#include "Os/RawTime.hpp"
#include "fprime-samd/Svc/Samd21Time/Samd21TimeComponentAc.hpp"

namespace Samd21 {

class Samd21Time final : public Samd21TimeComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Samd21Time object
    Samd21Time(const char* const compName  //!< The component name
    );

    //! Destroy Samd21Time object
    ~Samd21Time();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for getTime
    //!
    //! Port to retrieve time
    void getTime_handler(FwIndexType portNum,  //!< The port number
                         Fw::Time& time        //!< Reference to Time object
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_TIME
    //!
    //! Command to set the time
    void SET_TIME_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             Fw::TimeValue value) override;

  private:
    //! Initialized at startup, this is the "zero" reference clock for PROC_TIME
    const Os::RawTime m_proc_ref_time;

    //! This is the raw time reference time for the SC_TIME
    Os::RawTime m_base_raw_time;

    //! The is the base SC_TIME corresponding to the m_base_raw_time raw time
    Fw::Time m_base_time;
};

}  // namespace Samd21

#endif
