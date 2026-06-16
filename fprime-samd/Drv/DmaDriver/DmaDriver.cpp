// ======================================================================
// \title  DmaDriver.cpp
// \author tumbar
// \brief  cpp file for DmaDriver component implementation class
// ======================================================================

#include "Samd21/Drv/DmaDriver/DmaDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "Samd21/Drv/DmaDriver/DmaChannel.hpp"
#include "samd21/include/instance/dmac.h"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DmaDriver ::DmaDriver(const char* const compName)
    : DmaDriverComponentBase(compName),
      m_channels{
          DmaChannel(0), DmaChannel(1), DmaChannel(2), DmaChannel(3), DmaChannel(4),  DmaChannel(5),
          DmaChannel(6), DmaChannel(7), DmaChannel(8), DmaChannel(9), DmaChannel(10), DmaChannel(11),
      } {}

DmaDriver ::~DmaDriver() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DmaDriver ::configure_handler(FwIndexType portNum,
                                   const Samd21::DmaDriver_TriggerSource& trigger,
                                   const Samd21::DmaDriver_TransactionType& action,
                                   const Samd21::DmaDriver_BeatSize& beatSize,
                                   const Samd21::DmaDriver_Priority& priority,
                                   bool incrementSource,
                                   bool incrementDestination,
                                   const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                                   const Samd21::DmaDriver_StepSelection& stepSelection) {
    FW_ASSERT(portNum < DMAC_CH_NUM, portNum);

    m_channels[portNum].configure(trigger, action, beatSize, priority, incrementSource, incrementDestination, stepSize,
                                  stepSelection);
}

void DmaDriver ::start_handler(FwIndexType portNum, U32 sourceAddr, U32 destAddr, FwSizeType len) {
    FW_ASSERT(portNum < DMAC_CH_NUM, portNum);

    m_channels[portNum].start(reinterpret_cast<void*>(sourceAddr), reinterpret_cast<void*>(destAddr), len);
}

}  // namespace Samd21
