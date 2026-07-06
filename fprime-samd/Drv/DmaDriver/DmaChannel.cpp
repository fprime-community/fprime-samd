// ======================================================================
// \title  DmaChannel.cpp
// \author tumbar
// \brief  cpp file for DmaChannel helper class
// ======================================================================

#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "Os/Mutex.hpp"
#include "sam.h"

namespace Samd21 {

// Global descriptor memory - 16-byte aligned, placed in special section
__attribute__((__aligned__(16))) ::DmacDescriptor dmac_base[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

__attribute__((__aligned__(16))) ::DmacDescriptor dmac_writeback[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

// Global mutex for DMAC register access
static Os::Mutex s_dmac_mutex;

DmaChannel::DmaChannel(U8 channel_id) : m_channel_id(channel_id), m_busy(false), m_circular(false) {}

static void waitForChannelSuspend() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !(DMAC->CHINTFLAG.reg & DMAC_CHINTFLAG_SUSP)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void suspendChannel(U8 channel_id) {
    DMAC->CHID.reg = DMAC_CHID_ID(channel_id);
    DMAC->CHINTENCLR.reg = DMAC_CHINTENCLR_SUSP;
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_SUSPEND;
    waitForChannelSuspend();
}

static void resumeChannel() {
    DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;
    DMAC->CHINTENSET.reg = DMAC_CHINTENSET_SUSP;
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_RESUME;
}

void DmaChannel::startTransaction(const Samd21::Dma::TriggerSource& trigger,
                                  const Samd21::Dma::TransactionType& action,
                                  const Samd21::Dma::Priority& priority,
                                  DmacDescriptor* desc) {
    FW_ASSERT(!m_busy, m_channel_id);
    FW_ASSERT(desc != nullptr, m_channel_id);

    Os::ScopeLock lock(s_dmac_mutex);

    // Copy descriptor to base descriptor for this channel
    std::memcpy(reinterpret_cast<void*>(&dmac_base[m_channel_id]), desc, sizeof(DmacDescriptor));

    // Configure channel
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
    DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;  // Reset channel

    // Clear software trigger
    DMAC->SWTRIGCTRL.reg &= ~(1 << m_channel_id);

    // Configure priority, trigger source, and trigger action
    DMAC->CHCTRLB.reg = DMAC_CHCTRLB_LVL(priority.e) | DMAC_CHCTRLB_TRIGSRC(trigger.e) | DMAC_CHCTRLB_TRIGACT(action.e);

    // Enable interrupts for this channel
    DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL | DMAC_CHINTENSET_TERR | DMAC_CHINTENSET_SUSP;

    // Enable the channel
    DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;

    // Issue software trigger to initiate first transfer if peripheral trigger condition is already met
    // Per datasheet §21.8.8: Writing '1' to SWTRIGn generates a DMA trigger if CHSTATUS.PEND=0
    DMAC->SWTRIGCTRL.reg |= (1 << m_channel_id);

    m_busy = true;
}

void DmaChannel::appendToChain(DmacDescriptor* newDesc) {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(newDesc != nullptr, m_channel_id);

    // We cannot add a transaction to a circular chain (there is no end)
    FW_ASSERT(!this->m_circular, m_channel_id);

    Os::ScopeLock lock(s_dmac_mutex);

    // Select this channel
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);

    // Check if the channel completed before we could append (race condition)
    // If SUSP interrupt is pending, the descriptor chain has ended
    if (DMAC->CHINTFLAG.reg & DMAC_CHINTFLAG_SUSP) {
        // Channel already completed and suspended - we missed our chance to append to the chain
        // The new descriptor must become the new base descriptor
        // Copy the new descriptor to the base
        std::memcpy(reinterpret_cast<void*>(&dmac_base[m_channel_id]), newDesc, sizeof(DmacDescriptor));

        // Clear SUSP flag and resume - this restarts the channel with the new base descriptor
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;
        DMAC->CHINTENSET.reg = DMAC_CHINTENSET_SUSP;  // Re-enable SUSP interrupt
        DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_RESUME;

        // Issue software trigger to restart
        DMAC->SWTRIGCTRL.reg |= (1 << m_channel_id);
        return;
    }

    // Normal case: channel is still executing, suspend and append to chain
    suspendChannel(m_channel_id);

    // Find last descriptor in chain starting from the base descriptor for this channel
    ::DmacDescriptor* lastDesc = &dmac_base[m_channel_id];
    while (lastDesc->DESCADDR.reg != 0) {
        // Hardware stores descriptor addresses as uint32_t - reinterpret to pointer
        lastDesc = reinterpret_cast<::DmacDescriptor*>(lastDesc->DESCADDR.reg);
    }

    // Link new descriptor - hardware requires pointer as uint32_t
    lastDesc->DESCADDR.reg = reinterpret_cast<uint32_t>(newDesc);

    resumeChannel();
}

void DmaChannel::queueTransaction(const Samd21::Dma::TriggerSource& trigger,
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
    } else {
        // Channel busy - append to linked list
        appendToChain(desc);
    }
}

void DmaChannel::linkToFront() {
    FW_ASSERT(m_busy, m_channel_id);

    Os::ScopeLock lock(s_dmac_mutex);

    suspendChannel(m_channel_id);

    // Must have at least one descriptor to make a circular chain
    ::DmacDescriptor* base = &dmac_base[m_channel_id];
    FW_ASSERT(base->DESCADDR.reg != 0, m_channel_id);  // Must have at least one linked descriptor

    // Find the last descriptor in the chain
    ::DmacDescriptor* lastDesc = base;
    while (lastDesc->DESCADDR.reg != 0) {
        lastDesc = reinterpret_cast<::DmacDescriptor*>(lastDesc->DESCADDR.reg);
    }

    // Point last descriptor back to base to create circular buffer
    lastDesc->DESCADDR.reg = reinterpret_cast<uint32_t>(base);

    // Mark channel as circular to prevent descriptor freeing
    m_circular = true;

    resumeChannel();
}

void DmaChannel::popFront(Samd21::Dma::Writeback& result) {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(m_circular, m_channel_id);  // Only valid for circular buffers

    Os::ScopeLock lock(s_dmac_mutex);

    suspendChannel(m_channel_id);

    // Read writeback descriptor for this channel
    auto wb = &dmac_writeback[m_channel_id];

    result.set_btctrl(wb->BTCTRL.reg);
    result.set_btcnt(wb->BTCNT.reg);
    result.set_srcaddr(wb->SRCADDR.reg);
    result.set_dstaddr(wb->DSTADDR.reg);
    result.set_descaddr(wb->DESCADDR.reg);

    // Skip to next descriptor by marking current descriptor as complete
    // Setting BTCNT to 0 forces the DMA to move to the next descriptor
    if (wb->DESCADDR.reg != 0) {
        wb->BTCNT.reg = 0;  // Mark current descriptor as "complete"
    }

    resumeChannel();
}

void DmaChannel::suspend() {
    if (!m_busy) {
        return;  // Nothing to suspend
    }

    Os::ScopeLock lock(s_dmac_mutex);

    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_SUSPEND;
}

void DmaChannel::markIdle() {
    m_busy = false;
}

U8 DmaChannel::getBeatSizeBytes() const {
    FW_ASSERT(m_busy, m_channel_id);

    // Extract beat size from the base descriptor BTCTRL for this channel
    U8 beatSizeEnum = (dmac_base[m_channel_id].BTCTRL.reg >> DMAC_BTCTRL_BEATSIZE_Pos) & 0x3;
    return static_cast<U8>(1U << beatSizeEnum);
}

}  // namespace Samd21
