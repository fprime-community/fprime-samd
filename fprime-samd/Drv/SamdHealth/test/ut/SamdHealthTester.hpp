// ======================================================================
// \title  SamdHealthTester.hpp
// \author tumbar
// \brief  hpp file for SamdHealth test harness implementation class
// ======================================================================

#ifndef Samd21_SamdHealthTester_HPP
#define Samd21_SamdHealthTester_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "fprime-samd/Drv/SamdHealth/SamdHealth.hpp"
#include "fprime-samd/Drv/SamdHealth/SamdHealthGTestBase.hpp"
#include "fprime-samd/Drv/SamdHealth/SamdHealthHardware.hpp"

namespace Samd21 {

class SamdHealthTester : public SamdHealthGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Command sequence number used by tests that do not care about the value
    static constexpr U32 TEST_CMD_SEQ = 0;

    // Construction and destruction
    SamdHealthTester();
    ~SamdHealthTester();

    // Tests
    void testEmitSystemInfoReportsResetReason();
    void testCommitStampsAreReported();
    void testHardwareQueriedOncePerCommand();
    void testRepeatedEmit();

  private:
    //! Component under test
    SamdHealth component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Reset test and stub hardware state between tests
    void resetTest();

    //! Issue EMIT_SYSTEM_INFO and assert the event, every channel and the command
    //! response all carry the expected reset reason and build commits
    //! \param reason Reset reason the stub HAL is reporting
    //! \param cmdSeq Command sequence number to send
    void assertEmitReports(ResetReason reason, U32 cmdSeq);
};

}  // namespace Samd21

#endif
