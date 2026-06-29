// ======================================================================
// \title  reset_assert_hooks.cpp
// \author Andrei Tumbar
// \brief  cpp file for assert hook functions using send/reset implementation
// ======================================================================
#include <Fw/Types/assert_hook.hpp>
#include <Fw/Types/format.hpp>

#include <sam.h>
#include "../Mcu/Delay.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Platform/PlatformTypes.h"
#include "config/FatalPacketSerializableAc.hpp"
#include "config/FatalTimeSerializableAc.hpp"
#include "config/FwSizeTypeAliasAc.h"

#if FW_ASSERT_LEVEL == FW_FILEID_ASSERT
#define fileIdFs "Assert: 0x%08" PRIx32 ":%" PRI_FwSizeType ""
#else
#define fileIdFs "Assert: \"%s:%" PRI_FwSizeType "\""
#endif

namespace Samd21 {
extern void sendFatalPacket(Fw::ComBuffer& data);
extern void sendBailFrame(FILE_NAME_ARG file, FwSizeType lineNo);
}  // namespace Samd21

extern "C" void HardFault_Handler(void) {
    Samd21::sendBailFrame(0x0, 0x0);
    Fw::defaultDoAssert();
}

void Fw::defaultDoAssert() {
    // Hold for 10s to allow flashing without reinitializing the USB
    // TODO(tumbar) We will want to disable RTC interrupts here
    __disable_irq();
    NVIC_SystemReset();

    while (true) {
    }
}

void Fw::defaultPrintAssert(const CHAR* msg) {
    auto ptr = reinterpret_cast<const U8*>(reinterpret_cast<PlatformPointerCastType>(msg));

    Fw::ComBuffer frame(const_cast<U8*>(ptr), static_cast<FwSizeType>(Samd21::FatalPacket::SERIALIZED_SIZE));

    for (int i = 0; i < 10; i++) {
        // Drv::framerSendComPacket(frame);
        Samd21::sendFatalPacket(frame);
        delay(1000);
    }
}

void trigger_breakpoint() {
    __asm__ volatile ("BKPT #0");
}

void Fw::defaultReportAssert(FILE_NAME_ARG file,
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
    trigger_breakpoint();

    static volatile bool assertReached = false;
    if (assertReached) {
        // Assert within assert!
        for (int i = 0; i < 10; i++) {
            Samd21::sendBailFrame(file, lineNo);
            delay(1000);
        }
        delay(1000);
        Fw::defaultDoAssert();
        return;
    }

    assertReached = true;

    Fw::ExternalSerializeBuffer writer(reinterpret_cast<U8*>(destBuffer), buffSize);

    // Serialize the file location
    Samd21::FatalPacket p(
        ComCfg::Apid::FW_PACKET_LOG, 0x0, Samd21::FatalTime(),
        Samd21::FatalData(static_cast<U32>(file), static_cast<U32>(lineNo), static_cast<FwSizeStoreType>(numArgs),
                          static_cast<I32>(arg1), static_cast<I32>(arg2), static_cast<I32>(arg3),
                          static_cast<I32>(arg4), static_cast<I32>(arg5), static_cast<I32>(arg6)));
    auto status = p.serializeTo(writer);
    FW_ASSERT(status == FW_SERIALIZE_OK, status);
}
