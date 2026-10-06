// ======================================================================
// \title  SystemInfoTester.hpp
// \author tumbar
// \brief  hpp file for SystemInfo test harness implementation class
// ======================================================================

#ifndef Samd21_SystemInfoTester_HPP
#define Samd21_SystemInfoTester_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfo.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfoGTestBase.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfoHardware.hpp"

namespace Samd21 {

class SystemInfoTester : public SystemInfoGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Construction and destruction
    SystemInfoTester();
    ~SystemInfoTester();

    // Tests
    void testEmitSystemInfoReportsResetReason();
    void testCommitStampsAreReported();
    void testHardwareQueriedOncePerCommand();
    void testRepeatedEmit();

  private:
    //! Component under test
    SystemInfo component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Reset test and stub hardware state between tests
    void resetTest();

    //! Call emit() and assert the event and every channel carry the expected reset
    //! reason and build commits
    //! \param reason Reset reason the stub HAL is reporting
    void assertEmitReports(ResetReason reason);
};

}  // namespace Samd21

#endif
