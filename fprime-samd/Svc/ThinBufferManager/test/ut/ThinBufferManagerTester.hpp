// ======================================================================
// \title  ThinBufferManager/test/ut/ThinBufferManagerTester.hpp
// \brief  hpp file for ThinBufferManager test harness implementation class
//
// Ported from lib/fprime/Svc/BufferManager/test/ut/BufferManagerTester.hpp,
// adapted for Samd21::ThinBufferManagerComponentImpl.
//
// ======================================================================

#ifndef Samd21_ThinBufferManagerTester_HPP
#define Samd21_ThinBufferManagerTester_HPP

#include <STest/Pick/Pick.hpp>
#include "fprime-samd/Svc/ThinBufferManager/ThinBufferManagerComponentImpl.hpp"
#include "fprime-samd/Svc/ThinBufferManager/ThinBufferManagerGTestBase.hpp"

namespace Samd21 {

class ThinBufferManagerTester : public ThinBufferManagerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ThinBufferManagerTester
    //!
    ThinBufferManagerTester();

    //! Destroy object ThinBufferManagerTester
    //!
    ~ThinBufferManagerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test Setup
    //!
    void testSetup();

    //! One buffer size
    void oneBufferSize();

    //! Multiple buffer sizes
    void multBuffSize();

  private:
    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    // Declared here, but defined in the generated ThinBufferManagerTesterHelpers.cpp
    // (UT_AUTO_HELPERS in CMakeLists.txt) rather than hand-written -- matching
    // fprime-samd/Svc/Framer's convention.

    //! Connect ports
    //!
    void connectPorts();

    //! Initialize components
    //!
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Variables
    // ----------------------------------------------------------------------

    //! The component under test
    //!
    ThinBufferManagerComponentImpl component;
};

}  // end namespace Samd21

#endif
