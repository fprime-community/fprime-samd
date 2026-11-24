// ======================================================================
// \title  FprimeRouterTester.hpp
// \author tumbar
// \brief  hpp file for FprimeRouter component test harness implementation class
// ======================================================================

#ifndef Samd21_FprimeRouterTester_HPP
#define Samd21_FprimeRouterTester_HPP

#include "fprime-samd/Svc/FprimeRouter/FprimeRouter.hpp"
#include "fprime-samd/Svc/FprimeRouter/FprimeRouterGTestBase.hpp"

namespace Samd21 {

class FprimeRouterTester final : public FprimeRouterGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static constexpr FwSizeType MAX_HISTORY_SIZE = 16;

    // Instance ID supplied to the component instance under test
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Virtual channel stamped on received packets, distinct from the FrameContext default
    static constexpr U8 TEST_VC_ID = 3;

    //! Size of the packet storage backing the test buffers
    static constexpr FwSizeType PACKET_CAPACITY = FW_COM_BUFFER_MAX_SIZE + 16;

    //! Which optional output ports the harness leaves unconnected
    enum class Optional { CONNECTED, UNCONNECTED };

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    explicit FprimeRouterTester(Optional optional = Optional::CONNECTED);
    ~FprimeRouterTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testRouteCommand();
    void testRouteCommandMaxSize();
    void testRouteCommandOversize();
    void testRouteCommandOversizeThrottle();
    void testRouteFile();
    void testRouteFileUnconnected();
    void testRouteUnknown();
    void testRouteUnknownUnconnected();
    void testBufferReturn();
    void testCommandResponse();
    void testRandomRouting();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Connect every port except the optional fileOut and unknownDataOut
    void connectPortsExceptOptional();

    //! Fill the first `size` bytes of the packet storage with random data and send
    //! them on dataIn with the given APID and TEST_VC_ID
    //! \return The buffer sent on dataIn
    Fw::Buffer sendPacket(ComCfg::Apid::T apid, FwSizeType size);

    //! Assert the output counts on each routed port and the return port
    void assertRouted(FwSizeType commands, FwSizeType files, FwSizeType unknown, FwSizeType returns);

    //! Assert the buffer at `index` of the dataReturnOut history is `buffer`, with a
    //! context whose vcId and apid are `vcId` and `apid`
    void assertReturned(FwSizeType index, const Fw::Buffer& buffer, U8 vcId, ComCfg::Apid::T apid);

    //! Assert the command at `index` of the commandOut history carries `size` bytes of `buffer`
    void assertCommand(FwSizeType index, const Fw::Buffer& buffer);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    FprimeRouter component;

    //! Storage backing the buffers sent on dataIn
    U8 m_packet[PACKET_CAPACITY];
};

}  // namespace Samd21

#endif
