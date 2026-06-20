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
__attribute__((__aligned__(16))) DmacDescriptor dmac_base[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

__attribute__((__aligned__(16))) DmacDescriptor dmac_writeback[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;

// Global mutex for DMAC register access
static Os::Mutex s_dmac_mutex;

DmaChannel::DmaChannel(U8 channel_id) : m_channel_id(channel_id), m_busy(false) {}

static void waitForChannelSuspend() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !(DMAC->CHINTFLAG.reg & DMAC_CHINTFLAG_SUSP)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
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

    m_busy = true;
}

void DmaChannel::appendToChain(DmacDescriptor* newDesc) {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(newDesc != nullptr, m_channel_id);

    Os::ScopeLock lock(s_dmac_mutex);

    // Suspend channel to safely modify descriptor chain
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_SUSPEND;

    // Wait for suspend to complete with timeout protection
    waitForChannelSuspend();

    // Find last descriptor in chain starting from the base descriptor for this channel
    DmacDescriptor* lastDesc = &dmac_base[m_channel_id];
    while (lastDesc->DESCADDR.reg != 0) {
        // Hardware stores descriptor addresses as uint32_t - reinterpret to pointer
        lastDesc = reinterpret_cast<DmacDescriptor*>(lastDesc->DESCADDR.reg);
    }

    // Link new descriptor - hardware requires pointer as uint32_t
    lastDesc->DESCADDR.reg = reinterpret_cast<uint32_t>(newDesc);

    // Clear suspend flag
    DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;

    // Resume channel
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_RESUME;
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
