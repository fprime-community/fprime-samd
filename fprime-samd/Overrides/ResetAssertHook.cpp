// ======================================================================
// \title  ResetAssertHook.cpp
// \author Andrei Tumbar
// \brief  cpp file for the send/reset assert hook
// ======================================================================
#include "ResetAssertHook.hpp"

#include <sam.h>
#include "../Mcu/Delay.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Platform/PlatformTypes.h"
#include "config/FatalPacketSerializableAc.hpp"
#include "config/FatalTimeSerializableAc.hpp"
#include "config/FwSizeTypeAliasAc.h"

extern "C" __attribute__((used)) void HardFault_Handler(void) {
    // __BKPT(3);
    NVIC_SystemReset();
}

namespace Samd21 {
extern void sendFatalPacket(Fw::ComBuffer& data);
extern void sendBailFrame(FILE_NAME_ARG file, FwSizeType lineNo);

void ResetAssertHook::doAssert() {
    NVIC_SystemReset();

    while (true) {
    }
}

void ResetAssertHook::printAssert(const CHAR* msg) {
    __disable_irq();

    static volatile bool assertReached = false;
    if (assertReached) {
        this->doAssert();
    }

    assertReached = true;

    auto ptr = reinterpret_cast<const U8*>(reinterpret_cast<PlatformPointerCastType>(msg));
    Fw::ComBuffer frame(const_cast<U8*>(ptr), static_cast<FwSizeType>(Samd21::FatalPacket::SERIALIZED_SIZE));

    for (int i = 0; i < 10; i++) {
        Samd21::sendFatalPacket(frame);
        delay(1000);
    }
}

void ResetAssertHook::reportAssert(FILE_NAME_ARG file,
                                   FwSizeType lineNo,
                                   FwSizeType numArgs,
                                   FwAssertArgType arg1,
                                   FwAssertArgType arg2,
                                   FwAssertArgType arg3,
                                   FwAssertArgType arg4,
                                   FwAssertArgType arg5,
                                   FwAssertArgType arg6) {
    U8 destBuffer[Samd21::FatalPacket::SERIALIZED_SIZE];
    Fw::ExternalSerializeBuffer writer(destBuffer, static_cast<FwSizeType>(sizeof(destBuffer)));

    // Serialize the file location
    Samd21::FatalPacket p(
        ComCfg::Apid::FW_PACKET_LOG, 0x0, Samd21::FatalTime(),
        Samd21::FatalData(static_cast<U32>(file), static_cast<U32>(lineNo), static_cast<FwSizeStoreType>(numArgs),
                          static_cast<I32>(arg1), static_cast<I32>(arg2), static_cast<I32>(arg3),
                          static_cast<I32>(arg4), static_cast<I32>(arg5), static_cast<I32>(arg6)));
    auto status = p.serializeTo(writer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    this->printAssert(reinterpret_cast<const CHAR*>(destBuffer));
}

}  // namespace Samd21
