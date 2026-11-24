// ======================================================================
// \title  FatalHandler.cpp
// \author tumbar
// \brief  cpp file for FatalHandler component implementation class
// ======================================================================

#include "fprime-samd/Svc/FatalHandler/FatalHandler.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Time/Time.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/ApidEnumAc.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwPacketDescriptorTypeAliasAc.h"
#include "config/FwSizeTypeAliasAc.h"
#include "fprime-samd/Svc/FatalHandler/FatalHandlerHardware.hpp"
#include "samd-config/FatalHandlerConfig.hpp"

namespace Samd21 {

//! Number of assert arguments carried by the AssertionFailed event
static constexpr FwSizeType ASSERT_ARG_COUNT = 6;

//! Size of the AssertionFailed packet: an Fw::LogPacket header (descriptor, event ID,
//! time tag) followed by the event's arguments (file, lineNo, numArgs, arg1..arg6)
static constexpr FwSizeType ASSERT_PACKET_SIZE = sizeof(FwPacketDescriptorType) + sizeof(FwEventIdType) +
                                                 Fw::Time::SERIALIZED_SIZE + sizeof(U32) + sizeof(U32) +
                                                 sizeof(FwSizeStoreType) + ASSERT_ARG_COUNT * sizeof(I32);

static_assert(ASSERT_PACKET_SIZE <= FW_COM_BUFFER_MAX_SIZE, "AssertionFailed packet must fit in a Fw::ComBuffer");
static_assert(ASSERT_PACKET_SIZE <= FW_ASSERT_TEXT_SIZE,
              "AssertionFailed packet must fit in the assert message buffer (FwAssertTextSize)");

static FatalHandler* s_fatal_handler = nullptr;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FatalHandler::FatalHandler(const char* const compName)
    : FatalHandlerComponentBase(compName), m_assertReached(false), m_packetSize(0) {
    s_fatal_handler = this;
}

FatalHandler::~FatalHandler() {
    if (s_fatal_handler == this) {
        s_fatal_handler = nullptr;
    }
}

// ----------------------------------------------------------------------
// Assert hook entry points
// ----------------------------------------------------------------------

void FatalHandler::formatAssert(FILE_NAME_ARG file,
                                FwSizeType lineNo,
                                FwSizeType numArgs,
                                FwAssertArgType arg1,
                                FwAssertArgType arg2,
                                FwAssertArgType arg3,
                                FwAssertArgType arg4,
                                FwAssertArgType arg5,
                                FwAssertArgType arg6,
                                CHAR* destBuffer,
                                FwSizeType buffSize) {
    FatalHandlerHardware::FatalHandlerHal::disableInterrupts();

    FatalHandler* const handler = s_fatal_handler;
    if (handler == nullptr) {
        return;
    }

    // An assert raised while reporting (e.g. in the framer) lands back here;
    // give up on reporting and reset
    if (handler->m_assertReached) {
        handler->m_packetSize = 0;
        terminate();
        return;
    }
    handler->m_assertReached = true;

    // Read the time once so every copy of the packet carries the same time. The
    // guard is already set, so an assert raised in the time path resets.
    const Fw::Time time = handler->getTime();

#if FW_ASSERT_LEVEL == FW_FILEID_ASSERT
    const U32 fileId = static_cast<U32>(file);
#else
    // File names do not fit the event's U32 file argument
    const U32 fileId = 0;
#endif

    const FwAssertArgType args[ASSERT_ARG_COUNT] = {arg1, arg2, arg3, arg4, arg5, arg6};

    // Serialize in the Fw::LogPacket layout, the same as the downlink's events, so the
    // ground decodes the packet as the dictionary's AssertionFailed event
    Fw::ExternalSerializeBuffer writer(reinterpret_cast<U8*>(destBuffer), buffSize);
    (void)writer.serializeFrom(static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_LOG));
    (void)writer.serializeFrom(static_cast<FwEventIdType>(handler->getIdBase() + EVENTID_ASSERTIONFAILED));
    (void)writer.serializeFrom(time);
    (void)writer.serializeFrom(fileId);
    (void)writer.serializeFrom(static_cast<U32>(lineNo));
    (void)writer.serializeFrom(static_cast<FwSizeStoreType>(numArgs));
    for (FwSizeType i = 0; i < ASSERT_ARG_COUNT; i++) {
        (void)writer.serializeFrom(static_cast<I32>(args[i]));
    }

    handler->m_packetSize = writer.getSize();
}

void FatalHandler::sendAssert(const CHAR* msg) {
    FatalHandler* const handler = s_fatal_handler;
    if ((handler == nullptr) || (handler->m_packetSize == 0)) {
        return;
    }
    const FwSizeType packetSize = handler->m_packetSize;
    handler->m_packetSize = 0;

    if (!handler->isConnected_fatalOut_OutputPort(0)) {
        return;
    }

    Fw::ComBuffer frame(reinterpret_cast<const U8*>(msg), packetSize);
    for (FwIndexType i = 0; i < FatalHandlerConfig::PACKET_REPEAT; i++) {
        handler->fatalOut_out(0, frame, 0);
        FatalHandlerHardware::FatalHandlerHal::delayMs(FatalHandlerConfig::PACKET_DELAY_MS);
    }
}

void FatalHandler::terminate() {
    FatalHandlerHardware::FatalHandlerHal::reset();
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void FatalHandler::FatalReceive_handler(const FwIndexType portNum, /*!< The port number*/
                                        FwEventIdType Id           /*!< The ID of the FATAL event*/
) {
    FW_ASSERT(0, static_cast<FwAssertArgType>(portNum), static_cast<FwAssertArgType>(Id));
}

}  // namespace Samd21
