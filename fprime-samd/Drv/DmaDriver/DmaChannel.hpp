// ======================================================================
// \title  DmaDriver.hpp
// \author tumbar
// \brief  hpp file for DmaChannel helper class
// ======================================================================

#ifndef Samd21_DmaChannel_HPP
#define Samd21_DmaChannel_HPP

#include "config/FwIndexTypeAliasAc.h"

#include "Samd21/Drv/DmaDriver/DmaDriver_AddressIncrementStepSizeEnumAc.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriver_BeatSizeEnumAc.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriver_PriorityEnumAc.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriver_StepSelectionEnumAc.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriver_TransactionTypeEnumAc.hpp"
#include "Samd21/Drv/DmaDriver/DmaDriver_TriggerSourceEnumAc.hpp"

namespace Samd21 {

class DmaChannel final {
  public:
    DmaChannel(U8 channel_id);

    //! Configure this channel's DMA transaction settings
    void configure(const Samd21::DmaDriver_TriggerSource& trigger,
                   const Samd21::DmaDriver_TransactionType& action,
                   const Samd21::DmaDriver_BeatSize& beatSize,
                   const Samd21::DmaDriver_Priority& priority,
                   bool incrementSource,
                   bool incrementDestination,
                   const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                   const Samd21::DmaDriver_StepSelection& stepSelection);

    //! Start a new DMA transaction on this channel
    //! Note: configure() must be called before this
    void start(void* sourceAddr, void* destAddr, FwSizeType len);

  private:
    U8 m_channel_id;
    bool m_configured;
    bool m_busy;
};

}  // namespace Samd21

#endif