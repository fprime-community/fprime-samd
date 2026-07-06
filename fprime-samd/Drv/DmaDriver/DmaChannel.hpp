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
#include "fprime-samd/Drv/Types/WritebackSerializableAc.hpp"

namespace Samd21 {

struct DmacDescriptor {
    U16 btctrl;
    U16 btcnt;
    U32 srcaddr;
    U32 dstaddr;
    U32 descaddr;
};

class DmaChannel final {
  public:
    DmaChannel();

    //! Set the channel ID (called during initialization)
    void setChannelId(U8 channel_id) { m_channel_id = channel_id; }

    //! Queue a DMA transaction on this channel
    //! If channel is idle, starts immediately. If busy, appends to linked descriptor chain.
    void queueTransaction(const Samd21::Dma::TriggerSource& trigger,
                          const Samd21::Dma::TransactionType& action,
                          const Samd21::Dma::Priority& priority,
                          DmacDescriptor* descriptor);

    //! Link the last descriptor back to the first to create a circular buffer
    //! The channel must have at least one linked descriptor already queued.
    //! Once called, the channel is marked as circular and descriptors will not be freed.
    void linkToFront();

    //! Suspend channel, read writeback, and advance to next descriptor
    //! Used for IDLE frame detection: process partial buffer and move to alternate.
    //! Returns the writeback state before advancing to the next descriptor.
    void popFront(Samd21::Dma::Writeback& result);

    //! Suspend this channel
    void suspend();

    //! Check if channel is busy
    bool isBusy() const { return m_busy; }

    //! Check if channel is in circular buffer mode
    bool isCircular() const { return m_circular; }

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
    bool m_circular;
};

}  // namespace Samd21

#endif