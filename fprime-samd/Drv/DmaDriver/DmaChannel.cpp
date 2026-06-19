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

DmaChannel::DmaChannel(U8 channel_id)
    : m_channel_id(channel_id), m_busy(false), m_descriptorUsed(0), m_currentBeatSize(DmaDriver_BeatSize::BYTE) {
    // Clear descriptor pool - intentional discard, always succeeds
    static_cast<void>(memset(m_descriptorPool, 0, sizeof(m_descriptorPool)));
}

U8 DmaChannel::getBeatSizeBytes() const {
    // Convert enum to bytes: BYTE=1, HWORD=2, WORD=4
    return static_cast<U8>(1U << m_currentBeatSize.e);
}

static void waitForChannelSuspend() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !(DMAC->CHINTFLAG.reg & DMAC_CHINTFLAG_SUSP)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static U32 calculateEndAddress(U32 startAddr, U32 len, bool increment) {
    if (increment) {
        return startAddr + len;
    } else {
        return startAddr;
    }
}

void DmaChannel::setupDescriptor(DmacDescriptor* desc,
                                 U32 sourceAddr,
                                 U32 destAddr,
                                 U32 len,
                                 const Samd21::DmaDriver_BeatSize& beatSize,
                                 bool incrementSource,
                                 bool incrementDestination,
                                 const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                                 const Samd21::DmaDriver_StepSelection& stepSelection) {
    // Validate parameters
    FW_ASSERT(desc != nullptr, m_channel_id);
    FW_ASSERT(sourceAddr != 0, m_channel_id);
    FW_ASSERT(destAddr != 0, m_channel_id);

    U8 beatSizeBytes = static_cast<U8>(1U << beatSize.e);
    FW_ASSERT(sourceAddr % beatSizeBytes == 0, sourceAddr, beatSizeBytes);
    FW_ASSERT(destAddr % beatSizeBytes == 0, destAddr, beatSizeBytes);
    FW_ASSERT(len % beatSizeBytes == 0, len, beatSizeBytes);

    FwSizeType beatCount = len / beatSizeBytes;
    FW_ASSERT(beatCount > 0 && beatCount <= 65535, beatCount);

    // Setup BTCTRL - Block Transfer Control
    desc->BTCTRL.reg = DMAC_BTCTRL_VALID |         // Descriptor is valid
                       DMAC_BTCTRL_BLOCKACT_INT |  // Generate interrupt on completion
                       (beatSize.e << DMAC_BTCTRL_BEATSIZE_Pos) | (incrementSource ? DMAC_BTCTRL_SRCINC : 0) |
                       (incrementDestination ? DMAC_BTCTRL_DSTINC : 0) | (stepSelection.e << DMAC_BTCTRL_STEPSEL_Pos) |
                       (stepSize.e << DMAC_BTCTRL_STEPSIZE_Pos);

    // Setup beat count
    desc->BTCNT.reg = static_cast<uint16_t>(beatCount);

    // Setup addresses (END addresses if increment is enabled)
    desc->SRCADDR.reg = calculateEndAddress(sourceAddr, len, incrementSource);
    desc->DSTADDR.reg = calculateEndAddress(destAddr, len, incrementDestination);

    // No next descriptor by default
    desc->DESCADDR.reg = 0;
}

void DmaChannel::startTransaction(const Samd21::DmaDriver_TriggerSource& trigger,
                                  const Samd21::DmaDriver_TransactionType& action,
                                  const Samd21::DmaDriver_Priority& priority,
                                  U32 sourceAddr,
                                  U32 destAddr,
                                  U32 len,
                                  const Samd21::DmaDriver_BeatSize& beatSize,
                                  bool incrementSource,
                                  bool incrementDestination,
                                  const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                                  const Samd21::DmaDriver_StepSelection& stepSelection) {
    FW_ASSERT(!m_busy, m_channel_id);

    Os::ScopeLock lock(s_dmac_mutex);

    // Store current beat size for later calculations
    m_currentBeatSize = beatSize;

    // Setup base descriptor
    DmacDescriptor* desc = &dmac_base[m_channel_id];
    setupDescriptor(desc, sourceAddr, destAddr, len, beatSize, incrementSource, incrementDestination, stepSize,
                    stepSelection);

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
    m_descriptorUsed = 0;  // Base descriptor is managed separately
}

void DmaChannel::appendToChain(U32 sourceAddr,
                               U32 destAddr,
                               U32 len,
                               const Samd21::DmaDriver_BeatSize& beatSize,
                               bool incrementSource,
                               bool incrementDestination,
                               const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                               const Samd21::DmaDriver_StepSelection& stepSelection) {
    FW_ASSERT(m_busy, m_channel_id);
    FW_ASSERT(m_descriptorUsed < MAX_QUEUED_DESCRIPTORS, m_channel_id, m_descriptorUsed);

    Os::ScopeLock lock(s_dmac_mutex);

    // Allocate descriptor from pool
    DmacDescriptor* newDesc = &m_descriptorPool[m_descriptorUsed];
    m_descriptorUsed++;

    // Setup new descriptor
    setupDescriptor(newDesc, sourceAddr, destAddr, len, beatSize, incrementSource, incrementDestination, stepSize,
                    stepSelection);

    // Suspend channel to safely modify descriptor chain
    DMAC->CHID.reg = DMAC_CHID_ID(m_channel_id);
    DMAC->CHCTRLB.reg |= DMAC_CHCTRLB_CMD_SUSPEND;

    // Wait for suspend to complete with timeout protection
    waitForChannelSuspend();

    // Find last descriptor in chain
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

void DmaChannel::queueTransaction(const Samd21::DmaDriver_TriggerSource& trigger,
                                  const Samd21::DmaDriver_TransactionType& action,
                                  const Samd21::DmaDriver_Priority& priority,
                                  U32 sourceAddr,
                                  U32 destAddr,
                                  U32 len,
                                  const Samd21::DmaDriver_BeatSize& beatSize,
                                  bool incrementSource,
                                  bool incrementDestination,
                                  const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                                  const Samd21::DmaDriver_StepSelection& stepSelection) {
    // Validate all enums
    FW_ASSERT(trigger.isValid(), m_channel_id, trigger.e);
    FW_ASSERT(action.isValid(), m_channel_id, action.e);
    FW_ASSERT(beatSize.isValid(), m_channel_id, beatSize.e);
    FW_ASSERT(priority.isValid(), m_channel_id, priority.e);
    FW_ASSERT(stepSize.isValid(), m_channel_id, stepSize.e);
    FW_ASSERT(stepSelection.isValid(), m_channel_id, stepSelection.e);

    if (!m_busy) {
        // Channel idle - start new transaction
        startTransaction(trigger, action, priority, sourceAddr, destAddr, len, beatSize, incrementSource,
                         incrementDestination, stepSize, stepSelection);
    } else {
        // Channel busy - append to linked list
        appendToChain(sourceAddr, destAddr, len, beatSize, incrementSource, incrementDestination, stepSize,
                      stepSelection);
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
    m_descriptorUsed = 0;  // Reset descriptor pool
}

}  // namespace Samd21
