// ======================================================================
// \title  DmaChannel.hpp
// \author tumbar
// \brief  hpp file for DmaChannel helper class
// ======================================================================

#ifndef Samd21_DmaChannel_HPP
#define Samd21_DmaChannel_HPP

#include "fprime-samd/Drv/DmaDriver/DmaDriver_AddressIncrementStepSizeEnumAc.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriver_BeatSizeEnumAc.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriver_PriorityEnumAc.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriver_StepSelectionEnumAc.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriver_TransactionTypeEnumAc.hpp"
#include "fprime-samd/Drv/DmaDriver/DmaDriver_TriggerSourceEnumAc.hpp"
#include "sam.h"

namespace Samd21 {

// Maximum descriptors that can be queued per channel (beyond the base descriptor)
constexpr U8 MAX_QUEUED_DESCRIPTORS = 4;

class DmaChannel final {
  public:
    DmaChannel() : m_channel_id(0), m_busy(false), m_descriptorUsed(0), m_currentBeatSize(DmaDriver_BeatSize::BYTE) {}
    DmaChannel(U8 channel_id);

    //! Set the channel ID (called during initialization)
    void setChannelId(U8 channel_id) { m_channel_id = channel_id; }

    //! Queue a DMA transaction on this channel
    //! If channel is idle, starts immediately. If busy, appends to linked descriptor chain.
    void queueTransaction(const Samd21::DmaDriver_TriggerSource& trigger,
                          const Samd21::DmaDriver_TransactionType& action,
                          const Samd21::DmaDriver_Priority& priority,
                          U32 sourceAddr,
                          U32 destAddr,
                          U32 len,
                          const Samd21::DmaDriver_BeatSize& beatSize,
                          bool incrementSource,
                          bool incrementDestination,
                          const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                          const Samd21::DmaDriver_StepSelection& stepSelection);

    //! Suspend this channel
    void suspend();

    //! Check if channel is busy
    bool isBusy() const { return m_busy; }

    //! Mark channel as idle (called from ISR on completion)
    void markIdle();

    //! Get beat size in bytes for this channel's current configuration
    U8 getBeatSizeBytes() const;

  private:
    //! Calculate end address for descriptor (DMAC requires end address, not start)
    U32 calculateEndAddress(U32 startAddr, U32 len, bool increment) const;

    //! Setup a descriptor with transaction parameters
    void setupDescriptor(DmacDescriptor* desc,
                         U32 sourceAddr,
                         U32 destAddr,
                         U32 len,
                         const Samd21::DmaDriver_BeatSize& beatSize,
                         bool incrementSource,
                         bool incrementDestination,
                         const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                         const Samd21::DmaDriver_StepSelection& stepSelection);

    //! Start transaction on idle channel
    void startTransaction(const Samd21::DmaDriver_TriggerSource& trigger,
                          const Samd21::DmaDriver_TransactionType& action,
                          const Samd21::DmaDriver_Priority& priority,
                          U32 sourceAddr,
                          U32 destAddr,
                          U32 len,
                          const Samd21::DmaDriver_BeatSize& beatSize,
                          bool incrementSource,
                          bool incrementDestination,
                          const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                          const Samd21::DmaDriver_StepSelection& stepSelection);

    //! Append descriptor to end of linked list on busy channel
    void appendToChain(U32 sourceAddr,
                       U32 destAddr,
                       U32 len,
                       const Samd21::DmaDriver_BeatSize& beatSize,
                       bool incrementSource,
                       bool incrementDestination,
                       const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                       const Samd21::DmaDriver_StepSelection& stepSelection);

    U8 m_channel_id;
    bool m_busy;

    // Descriptor pool for linked list
    DmacDescriptor m_descriptorPool[MAX_QUEUED_DESCRIPTORS];
    U8 m_descriptorUsed;  // Number of descriptors currently in use

    // Store current beat size for address calculations
    Samd21::DmaDriver_BeatSize m_currentBeatSize;
};

}  // namespace Samd21

#endif