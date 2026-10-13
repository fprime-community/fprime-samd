// ======================================================================
// \title  ProcTimeTester.hpp
// \author tumbar
// \brief  hpp file for ProcTime test harness implementation class
// ======================================================================

#ifndef Samd21_ProcTimeTester_HPP
#define Samd21_ProcTimeTester_HPP

#include "fprime-samd/Svc/ProcTime/ProcTime.hpp"
#include "fprime-samd/Svc/ProcTime/ProcTimeGTestBase.hpp"

namespace Samd21 {

class ProcTimeTester : public ProcTimeGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Construction and destruction
    ProcTimeTester();
    ~ProcTimeTester();

    // Tests
    void testTimeBase();
    void testTimeTracksRawTime();
    void testTimeIsMonotonic();

  private:
    //! Component under test
    ProcTime component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Read Os::RawTime as processor time, independently of the component under test
    static Fw::Time rawTimeNow();

    //! Invoke the getTime port and return the time it produced
    Fw::Time getTime();
};

}  // namespace Samd21

#endif
