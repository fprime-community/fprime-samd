// ======================================================================
// \title  ResetAssertHook.hpp
// \brief  Fw::AssertHook that downlinks a FATAL packet and resets the MCU
// ======================================================================
#ifndef Samd21_ResetAssertHook_HPP
#define Samd21_ResetAssertHook_HPP

#include <Fw/Types/Assert.hpp>

namespace Samd21 {

//! \brief Assert hook for bare-metal SAMD21 deployments
//!
//! On assert, serializes the location and arguments into a Samd21::FatalPacket, pushes it out the synchronous
//! framer via Samd21::sendFatalPacket (supplied by the deployment topology) and then resets the MCU. Register it
//! with registerHook() before the topology is set up.
class ResetAssertHook : public Fw::AssertHook {
  public:
    void reportAssert(FILE_NAME_ARG file,
                      FwSizeType lineNo,
                      FwSizeType numArgs,
                      FwAssertArgType arg1,
                      FwAssertArgType arg2,
                      FwAssertArgType arg3,
                      FwAssertArgType arg4,
                      FwAssertArgType arg5,
                      FwAssertArgType arg6) override;
    void printAssert(const CHAR* msg) override;
    void doAssert() override;
};

}  // namespace Samd21

#endif
