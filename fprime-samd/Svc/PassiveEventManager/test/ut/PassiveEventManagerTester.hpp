// ======================================================================
// \title  PassiveEventManagerTester.hpp
// \author tumbar
// \brief  hpp file for PassiveEventManager component test harness implementation class
// ======================================================================

#ifndef Samd21_PassiveEventManagerTester_HPP
#define Samd21_PassiveEventManagerTester_HPP

#include "Fw/Log/LogBuffer.hpp"
#include "Fw/Time/Time.hpp"
#include "fprime-samd/Svc/PassiveEventManager/PassiveEventManager.hpp"
#include "fprime-samd/Svc/PassiveEventManager/PassiveEventManagerGTestBase.hpp"

namespace Samd21 {

class PassiveEventManagerTester final : public PassiveEventManagerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Bytes in a log packet ahead of the event arguments: descriptor, event ID, time tag
    static constexpr FwSizeType PACKET_HEADER_SIZE =
        sizeof(FwPacketDescriptorType) + sizeof(FwEventIdType) + Fw::Time::SERIALIZED_SIZE;

    //! Largest argument buffer that fits in a com buffer after the packet header
    static constexpr FwSizeType MAX_FITTING_ARGS = FW_COM_BUFFER_MAX_SIZE - PACKET_HEADER_SIZE;

    //! Number of random events sent by testRandomEvents
    static constexpr U32 RANDOM_EVENTS = 1000;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    PassiveEventManagerTester();
    ~PassiveEventManagerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testPacketLayout();
    void testEverySeverity();
    void testArgumentSizes();
    void testFatalAnnounced();
    void testFatalPacketBeforeAnnounce();
    void testRandomEvents();
    void testOversizedEventDropped();
    void testOversizedFatalAnnounced();
    void testUnconnectedOutputs();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Record the FATAL announcement and the number of packets sent before it
    void from_FatalAnnounce_handler(FwIndexType portNum, FwEventIdType Id) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Clear the port history and the announce bookkeeping
    void resetTest();

    //! Fill an argument buffer with `size` random bytes
    static void fillArgs(Fw::LogBuffer& args, FwSizeType size);

    //! Pick a random time tag
    static Fw::Time pickTime();

    //! Pick a random non-FATAL severity
    static Fw::LogSeverity pickNonFatal();

    //! Send an event through LogRecv
    void sendEvent(FwEventIdType id, const Fw::Time& time, Fw::LogSeverity severity, const Fw::LogBuffer& args);

    //! Assert packet `index` of the PktSend history is a log packet carrying the event
    void assertPacket(FwSizeType index, FwEventIdType id, const Fw::Time& time, const Fw::LogBuffer& args);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    PassiveEventManager component;

    //! PktSend history size when FatalAnnounce was last called
    FwSizeType m_packetsAtAnnounce;
};

}  // namespace Samd21

#endif
