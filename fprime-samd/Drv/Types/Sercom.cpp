// ======================================================================
// \title  Sercom.cpp
// \author tumbar
// \brief  cpp file for SERCOM ISR utilities
// ======================================================================

#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "Drv/Types/SercomKindEnumAc.hpp"
#include "Fw/Types/Assert.hpp"

namespace Samd21 {
Dma::TriggerSource SercomUtil::rxDmaTrigger(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Dma::TriggerSource::SERCOM0_RX;
        case SercomKind::SERCOM_1:
            return Dma::TriggerSource::SERCOM1_RX;
        case SercomKind::SERCOM_2:
            return Dma::TriggerSource::SERCOM2_RX;
        case SercomKind::SERCOM_3:
            return Dma::TriggerSource::SERCOM3_RX;
        case SercomKind::SERCOM_4:
            return Dma::TriggerSource::SERCOM4_RX;
        case SercomKind::SERCOM_5:
            return Dma::TriggerSource::SERCOM5_RX;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sercom));
            return Dma::TriggerSource::SERCOM0_RX;
    }
}

Dma::TriggerSource SercomUtil::txDmaTrigger(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Dma::TriggerSource::SERCOM0_TX;
        case SercomKind::SERCOM_1:
            return Dma::TriggerSource::SERCOM1_TX;
        case SercomKind::SERCOM_2:
            return Dma::TriggerSource::SERCOM2_TX;
        case SercomKind::SERCOM_3:
            return Dma::TriggerSource::SERCOM3_TX;
        case SercomKind::SERCOM_4:
            return Dma::TriggerSource::SERCOM4_TX;
        case SercomKind::SERCOM_5:
            return Dma::TriggerSource::SERCOM5_TX;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sercom));
            return Dma::TriggerSource::SERCOM0_TX;
    }
}

}  // namespace Samd21
