// ======================================================================
// \title  DmaChannel.hpp
// \author tumbar
// \brief  hpp file for DmaChannel helper class
// ======================================================================

#ifndef Samd21_DmaChannel_HPP
#define Samd21_DmaChannel_HPP

#include "fprime-samd/Drv/Types/PriorityEnumAc.hpp"
#include "fprime-samd/Drv/Types/TransactionTypeEnumAc.hpp"
#include "fprime-samd/Drv/Types/TriggerSourceEnumAc.hpp"

#include "samd.h"

namespace Samd21 {

class DmaChannel final {
  public:
    DmaChannel() : m_channel_id(0) {}
    DmaChannel(U8 channel_id);

    //! Set the channel ID (called during initialization)
    void setChannelId(U8 channel_id) { m_channel_id = channel_id; }

    //! Queue a DMA transaction on this channel
    //! If channel is idle, starts immediately. If busy, appends to linked descriptor chain.
    void queueTransaction(const Samd21::Dma::TriggerSource& trigger,
                          const Samd21::Dma::TransactionType& action,
                          const Samd21::Dma::Priority& priority,
                          DmacDescriptor* descriptor);

    //! Suspend this channel
    void suspend();

    //! Check if channel is busy
    bool isBusy() const { return m_busy; }

    //! Mark channel as idle (called from ISR on completion)
    void markIdle();

    //! Get beat size in bytes for this channel's current configuration
    U8 getBeatSizeBytes() const;

  private:
    //! Start transaction on idle channel with pre-configured descriptor
    void startTransaction(const Samd21::Dma::TriggerSource& trigger,
                          const Samd21::Dma::TransactionType& action,
                          const Samd21::Dma::Priority& priority,
                          DmacDescriptor* desc);

    //! Append descriptor to end of linked list on busy channel
    void appendToChain(DmacDescriptor* desc);

    U8 m_channel_id;
    bool m_busy;
};

}  // namespace Samd21

#endif