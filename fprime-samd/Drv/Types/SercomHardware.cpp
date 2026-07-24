// ======================================================================
// \title  SercomIsr.cpp
// \author tumbar
// \brief  cpp file for SERCOM ISR utilities
// ======================================================================

#include "Drv/Types/SercomKindEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "samd.h"

namespace Samd21 {
struct SercomCallback {
    void (*callback)(SercomKind, void*);
    void* data;
};

static SercomCallback sercomCallbacks[6] = {};

void SercomUtil::registerIsrHandler(SercomKind sercom, void (*callback)(SercomKind, void*), void* data) {
    FW_ASSERT(sercom.isValid());
    FW_ASSERT(sercomCallbacks[sercom.e].callback == nullptr);
}

Sercom* SercomUtil ::getHardware(SercomKind sercom) {
    switch (sercom.e) {
        case SercomKind::SERCOM_0:
            return SERCOM0;
        case SercomKind::SERCOM_1:
            return SERCOM1;
        case SercomKind::SERCOM_2:
            return SERCOM2;
        case SercomKind::SERCOM_3:
            return SERCOM3;
            // Some SAMD21 devices don't have these SERCOM ports
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            return SERCOM4;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            return SERCOM5;
#endif
        default:
            FW_ASSERT(false, sercom.e);
            return nullptr;
    }
}

}  // namespace Samd21

extern "C" __attribute__((used)) void SERCOM0_Handler(void) {
    if (Samd21::sercomCallbacks[0].callback) {
        Samd21::sercomCallbacks[0].callback(Samd21::SercomKind::SERCOM_0, Samd21::sercomCallbacks[0].data);
    }
}

extern "C" __attribute__((used)) void SERCOM1_Handler(void) {
    if (Samd21::sercomCallbacks[1].callback) {
        Samd21::sercomCallbacks[1].callback(Samd21::SercomKind::SERCOM_1, Samd21::sercomCallbacks[1].data);
    }
}

extern "C" __attribute__((used)) void SERCOM2_Handler(void) {
    if (Samd21::sercomCallbacks[2].callback) {
        Samd21::sercomCallbacks[2].callback(Samd21::SercomKind::SERCOM_2, Samd21::sercomCallbacks[2].data);
    }
}

extern "C" __attribute__((used)) void SERCOM3_Handler(void) {
    if (Samd21::sercomCallbacks[3].callback) {
        Samd21::sercomCallbacks[3].callback(Samd21::SercomKind::SERCOM_3, Samd21::sercomCallbacks[3].data);
    }
}

extern "C" __attribute__((used)) void SERCOM4_Handler(void) {
    if (Samd21::sercomCallbacks[4].callback) {
        Samd21::sercomCallbacks[4].callback(Samd21::SercomKind::SERCOM_4, Samd21::sercomCallbacks[4].data);
    }
}

extern "C" __attribute__((used)) void SERCOM5_Handler(void) {
    if (Samd21::sercomCallbacks[5].callback) {
        Samd21::sercomCallbacks[5].callback(Samd21::SercomKind::SERCOM_5, Samd21::sercomCallbacks[5].data);
    }
}
