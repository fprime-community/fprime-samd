// ======================================================================
// \title  DmaChannel.cpp
// \author tumbar
// \brief  cpp file for DmaChannel helper class
// ======================================================================

#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "config-samd/DmaDriverConfig.hpp"
#include "sam.h"

namespace Samd21 {

// Global descriptor memory - 16-byte aligned, placed in special section
__attribute__((__aligned__(16))) ::DmacDescriptor dmac_base[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

__attribute__((__aligned__(16))) ::DmacDescriptor dmac_writeback[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

DmaChannel::DmaChannel() : m_channel_id(0), m_busy(false), m_circular(false) {}

static void waitForChannelSuspend() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !(DMAC->CHINTFLAG.reg & DMAC_CHINTFLAG_SUSP)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForChannelDisable() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && DMAC->CHCTRLA.bit.ENABLE != 0) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void suspendChannel() {
    DMAC->CHINTENCLR.bit.SUSP = 1;
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_SUSPEND;
    waitForChannelSuspend();
}

static void resumeChannel() {
    DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;
    DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL | DMAC_CHINTENSET_TERR | DMAC_CHINTENSET_SUSP;
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_RESUME;
}

void DmaChannel::startTransaction(const Samd21::Dma::TriggerSource& trigger,
                                  const Samd21::Dma::TransactionType& action,
                                  const Samd21::Dma::Priority& priority,
                                  DmacDescriptor* desc) {
    FW_ASSERT(!m_busy, m_channel_id);
    FW_ASSERT(desc != nullptr, m_channel_id);

    // Copy descriptor to base descriptor for this channel
    std::memcpy(reinterpret_cast<void*>(&dmac_base[m_channel_id]), desc, sizeof(DmacDescriptor));

    // Configure channel
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    DMAC->CHCTRLA.bit.ENABLE = 0;
    DMAC->CHCTRLA.bit.SWRST = 1;
    waitForChannelDisable();

    // Clear software trigger
    DMAC->SWTRIGCTRL.reg &= ~(1U << m_channel_id);

    // Configure priority, trigger source, and trigger action
    DMAC->CHCTRLB.bit.LVL = priority.e;
    DMAC->CHCTRLB.bit.TRIGSRC = trigger.e;
    DMAC->CHCTRLB.bit.TRIGACT = action.e;

    // Verify configuration was written correctly
    FW_ASSERT(DMAC->CHCTRLB.bit.TRIGSRC == trigger.e, DMAC->CHCTRLB.bit.TRIGSRC, trigger.e);
    FW_ASSERT(DMAC->CHCTRLB.bit.TRIGACT == action.e, DMAC->CHCTRLB.bit.TRIGACT, action.e);

    // Enable interrupts for this channel
    DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL | DMAC_CHINTENSET_TERR | DMAC_CHINTENSET_SUSP;

    // Enable the channel
    DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;

    m_busy = true;
}

bool DmaChannel::appendToChain(DmacDescriptor* newDesc) {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(newDesc != nullptr, m_channel_id);

    // We cannot add a transaction to a circular chain (there is no end)
    FW_ASSERT(!this->m_circular, m_channel_id);

    // Select this channel
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);

    // Channel is still executing, suspend and append to chain
    suspendChannel();

    // Find last descriptor in chain starting from the base descriptor for this channel
    ::DmacDescriptor* lastDesc = &dmac_base[m_channel_id];
    U32 n = 0;
    while (n <= DmaDriverConfig::DMA_DESCRIPTOR_N && lastDesc->DESCADDR.reg != 0) {
        // Hardware stores descriptor addresses as uint32_t - reinterpret to pointer
        lastDesc = reinterpret_cast<::DmacDescriptor*>(lastDesc->DESCADDR.reg);
        n++;
    }

    // Make sure we didn't hit the descriptor bound
    FW_ASSERT(n <= DmaDriverConfig::DMA_DESCRIPTOR_N);
    FW_ASSERT(lastDesc->DESCADDR.reg == 0);

    // Link the new descriptor onto the tail's SRAM copy.
    lastDesc->DESCADDR.reg = reinterpret_cast<U32>(newDesc);

    // Patch the write-back if the DMA is currently executing the tail node.
    //
    // The DMAC does not re-read a descriptor's next-pointer from SRAM: when it
    // begins a block it prefetches that block's DESCADDR into the channel's
    // write-back copy (datasheet 21.6.2.5). So if the node currently loaded in
    // the write-back has DESCADDR==0, the hardware has already cached "no next"
    // and will terminate the chain (auto-disabling the channel, 21.6.2.6) the
    // instant it finishes -- our SRAM link above would be fetched too late and
    // the appended transaction would never transmit. Writing the same link into
    // the write-back closes that window: the hardware fetches our new descriptor
    // instead of terminating.
    //
    // wb->DESCADDR==0 means "executing the tail" for ANY chain length, not just a
    // single-node (n==0) chain. The previous code only patched the n==0 case, so
    // a tail append on a multi-node chain (the common case under sustained TX)
    // silently dropped the transaction. Gate on the write-back, not on n.
    auto wb = &dmac_writeback[m_channel_id];
    bool onTail = (wb->DESCADDR.reg == 0);
    if (onTail) {
        wb->DESCADDR.reg = reinterpret_cast<U32>(newDesc);
    }

    resumeChannel();

    return onTail;
}

bool DmaChannel::queueTransaction(const Samd21::Dma::TriggerSource& trigger,
                                  const Samd21::Dma::TransactionType& action,
                                  const Samd21::Dma::Priority& priority,
                                  DmacDescriptor* desc) {
    // Validate all enums
    FW_ASSERT(trigger.isValid(), m_channel_id, trigger.e);
    FW_ASSERT(action.isValid(), m_channel_id, action.e);
    FW_ASSERT(priority.isValid(), m_channel_id, priority.e);
    FW_ASSERT(desc != nullptr, m_channel_id);

    if (!m_busy) {
        // Channel idle - start new transaction
        startTransaction(trigger, action, priority, desc);
        return false;
    } else {
        // Channel busy - append to linked list
        return appendToChain(desc);
    }
}

void DmaChannel::linkToFront() {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(!m_circular, m_channel_id);

    // Select this channel
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);

    suspendChannel();

    // Must have at least one descriptor to make a circular chain
    ::DmacDescriptor* base = &dmac_base[m_channel_id];
    FW_ASSERT(base->DESCADDR.reg != 0, m_channel_id);  // Must have at least one linked descriptor

    // Find the last descriptor in the chain
    ::DmacDescriptor* lastDesc = base;
    U32 n = 0;
    while (n <= DmaDriverConfig::DMA_DESCRIPTOR_N && lastDesc->DESCADDR.reg != 0) {
        lastDesc = reinterpret_cast<::DmacDescriptor*>(lastDesc->DESCADDR.reg);
        n++;
    }

    // Make sure we didn't hit the descriptor bound
    FW_ASSERT(n <= DmaDriverConfig::DMA_DESCRIPTOR_N);

    // Point last descriptor back to base to create circular buffer
    lastDesc->DESCADDR.reg = reinterpret_cast<U32>(base);

    // Mark channel as circular to prevent descriptor freeing
    m_circular = true;

    resumeChannel();
}

void DmaChannel::readWriteback(Samd21::Dma::Writeback& result) {
    FW_ASSERT(m_busy, m_channel_id);

    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    suspendChannel();

    // Read writeback descriptor for this channel
    auto wb = &dmac_writeback[m_channel_id];

    result.set_btctrl(wb->BTCTRL.reg);
    result.set_btcnt(wb->BTCNT.reg);
    result.set_srcaddr(wb->SRCADDR.reg);
    result.set_dstaddr(wb->DSTADDR.reg);
    result.set_descaddr(wb->DESCADDR.reg);

    resumeChannel();
}

void DmaChannel::markIdle() {
    m_busy = false;
}

}  // namespace Samd21
