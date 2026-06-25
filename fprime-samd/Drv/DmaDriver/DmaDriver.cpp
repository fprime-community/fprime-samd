// ======================================================================
// \title  DmaDriver.cpp
// \author tumbar
// \brief  cpp file for DmaDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/DmaDriver/DmaDriver.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "config/DmaDriverConfig.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwIndexTypeAliasAc.h"
#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include "sam.h"

namespace Samd21 {

// Singleton instance for ISR access
DmaDriver* DmaDriver::s_instance = nullptr;

// External references to global descriptor memory (defined in DmaChannel.cpp)
extern ::DmacDescriptor dmac_base[DMAC_CH_NUM];
extern ::DmacDescriptor dmac_writeback[DMAC_CH_NUM];

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DmaDriver::DmaDriver(const char* const compName) : DmaDriverComponentBase(compName) {
    // Initialize each channel with its ID using setter
    for (U8 i = 0; i < DMAC_CH_NUM; i++) {
        m_channels[i].setChannelId(i);
        m_currentExecutingDesc[i] = nullptr;
    }

    // Register singleton for ISR access - pointer cast required for FW_ASSERT diagnostic
    FW_ASSERT(s_instance == nullptr, reinterpret_cast<FwAssertArgType>(s_instance));
    s_instance = this;
}

DmaDriver::~DmaDriver() {
    s_instance = nullptr;
}

// ----------------------------------------------------------------------
// Private helper functions
// ----------------------------------------------------------------------

static void waitForDmacReset() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && DMAC->CTRL.bit.SWRST) {
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

static void setupDescriptor(DmacDescriptor* desc,
                            U32 sourceAddr,
                            U32 destAddr,
                            U32 len,
                            const Samd21::Dma::BeatSize& beatSize,
                            bool incrementSource,
                            bool incrementDestination,
                            const Samd21::Dma::AddressIncrementStepSize& stepSize,
                            const Samd21::Dma::StepSelection& stepSelection) {
    // Validate parameters
    FW_ASSERT(desc != nullptr);
    FW_ASSERT(sourceAddr);
    FW_ASSERT(destAddr);

    U8 beatSizeBytes = static_cast<U8>(1U << beatSize.e);
    FW_ASSERT(sourceAddr % beatSizeBytes == 0, sourceAddr, beatSizeBytes);
    FW_ASSERT(destAddr % beatSizeBytes == 0, destAddr, beatSizeBytes);
    FW_ASSERT(len % beatSizeBytes == 0, len, beatSizeBytes);

    FwSizeType beatCount = len / beatSizeBytes;
    FW_ASSERT(beatCount > 0 && beatCount <= 65535, beatCount);

    auto* desc_hw = reinterpret_cast<::DmacDescriptor*>(desc);

    // Setup BTCTRL - Block Transfer Control
    desc_hw->BTCTRL.reg = DMAC_BTCTRL_VALID |         // Descriptor is valid
                          DMAC_BTCTRL_BLOCKACT_INT |  // Generate interrupt on completion
                          (beatSize.e << DMAC_BTCTRL_BEATSIZE_Pos) | (incrementSource ? DMAC_BTCTRL_SRCINC : 0) |
                          (incrementDestination ? DMAC_BTCTRL_DSTINC : 0) |
                          (stepSelection.e << DMAC_BTCTRL_STEPSEL_Pos) | (stepSize.e << DMAC_BTCTRL_STEPSIZE_Pos);

    // Setup beat count
    desc_hw->BTCNT.reg = static_cast<uint16_t>(beatCount);

    // Setup addresses (END addresses if increment is enabled)
    desc_hw->SRCADDR.reg = calculateEndAddress(sourceAddr, len, incrementSource);
    desc_hw->DSTADDR.reg = calculateEndAddress(destAddr, len, incrementDestination);

    // No next descriptor by default
    desc_hw->DESCADDR.reg = 0;
}

// ----------------------------------------------------------------------
// Public member functions
// ----------------------------------------------------------------------

void DmaDriver::freeDescriptor(DmacDescriptor* desc) {
    // Check if this descriptor is from our pool
    if (desc < m_descriptors || desc >= m_descriptors + DmaDriverConfig::DMA_DESCRIPTOR_N) {
        // Not from our pool (e.g., dmac_base[]), nothing to free
        return;
    }

    // Calculate bit index
    U32 index = static_cast<U32>(desc - m_descriptors);
    FW_ASSERT(index < DmaDriverConfig::DMA_DESCRIPTOR_N, index);

    // Clear the bit
    m_descriptors_used &= ~(1U << index);
}

void DmaDriver::freeChain(DmacDescriptor* chainStart) {
    auto current = reinterpret_cast<::DmacDescriptor*>(chainStart);

    while (current != nullptr && current->DESCADDR.reg != 0) {
        // Get next descriptor before freeing current
        auto next = reinterpret_cast<::DmacDescriptor*>(current->DESCADDR.reg);

        // Free current descriptor if it's from our pool
        freeDescriptor(reinterpret_cast<DmacDescriptor*>(current));

        current = next;
    }

    // Free the last descriptor (with DESCADDR == 0)
    if (current != nullptr) {
        freeDescriptor(reinterpret_cast<DmacDescriptor*>(current));
    }
}

void DmaDriver::configure() {
    FW_ASSERT(!m_initialized);

    // Enable DMAC clocks
    PM->AHBMASK.bit.DMAC_ = 1;
    PM->APBBMASK.bit.DMAC_ = 1;

    // Perform software reset
    DMAC->CTRL.bit.DMAENABLE = 0;
    DMAC->CTRL.bit.SWRST = 1;
    waitForDmacReset();

    // Setup descriptor base address and write-back section base address - hardware requires pointer as uint32_t
    DMAC->BASEADDR.reg = reinterpret_cast<uint32_t>(dmac_base);
    DMAC->WRBADDR.reg = reinterpret_cast<uint32_t>(dmac_writeback);

    // Clear descriptor memory - intentional discard, always succeeds
    static_cast<void>(memset(dmac_base, 0, sizeof(dmac_base)));
    static_cast<void>(memset(dmac_writeback, 0, sizeof(dmac_writeback)));

    // Enable DMA controller with all priority levels
    DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);

    // Enable DMA interrupt at lowest priority
    NVIC_EnableIRQ(DMAC_IRQn);
    NVIC_SetPriority(DMAC_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

    m_initialized = true;
}

void DmaDriver::handleInterrupt() {
    // Read which channel triggered the interrupt
    uint8_t id = DMAC->INTPEND.bit.ID;
    FW_ASSERT(id < DMAC_CH_NUM, id);

    // Select the channel
    DMAC->CHID.reg = DMAC_CHID_ID(id);
    uint8_t flags = DMAC->CHINTFLAG.reg;
    uint8_t status = DMAC->CHSTATUS.reg;

    // Handle Transfer Error (bus error)
    if (flags & DMAC_CHINTFLAG_TERR) {
        // Check if also invalid descriptor (programming error)
        if (status & DMAC_CHSTATUS_FERR) {
            // Invalid descriptor - should never happen
            FW_ASSERT(false, id);
        } else {
            // Pure bus error - report it
            ::DmacDescriptor* wb = &dmac_writeback[id];
            U32 beatSizeBytes = m_channels[id].getBeatSizeBytes();
            U32 remainingBytes = wb->BTCNT.reg * beatSizeBytes;

            Samd21::Dma::Reply reply;
            reply.set_status(Samd21::Dma::Status::BUS_ERROR);
            reply.set_remainingBytes(remainingBytes);

            this->transactionIsrOut_out(id, reply);
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TERR;

        // Channel is disabled by hardware on error
        // Only free descriptors if not in circular mode
        if (!m_channels[id].isCircular()) {
            // Free the currently executing descriptor and all remaining descriptors in chain
            if (m_currentExecutingDesc[id] != nullptr) {
                freeDescriptor(m_currentExecutingDesc[id]);
            }

            // Free remaining chain (everything after the failed descriptor)
            ::DmacDescriptor* wb = &dmac_writeback[id];
            if (wb->DESCADDR.reg != 0) {
                freeChain(reinterpret_cast<DmacDescriptor*>(wb->DESCADDR.reg));
            }
        }

        m_currentExecutingDesc[id] = nullptr;
        m_channels[id].markIdle();
    }

    // Handle Suspend
    if (flags & DMAC_CHINTFLAG_SUSP) {
        if (status & DMAC_CHSTATUS_FERR) {
            // Fetch error - check if end-of-chain or invalid descriptor
            ::DmacDescriptor* wb = &dmac_writeback[id];
            if (wb->DESCADDR.reg == 0x00000000) {
                // Normal end of linked list
                // All chained descriptors were already freed incrementally in TCMPL handler
                // Just mark the channel idle
                m_currentExecutingDesc[id] = nullptr;
                m_channels[id].markIdle();
            } else {
                // Invalid descriptor with non-zero DESCADDR - programming error
                FW_ASSERT(false, id, wb->DESCADDR.reg);
            }
        } else {
            // User-requested suspend (not completion)
            this->suspendIsrOut_out(id);
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;
    }

    // Handle Transfer Complete (intermediate descriptor or final descriptor)
    if (flags & DMAC_CHINTFLAG_TCMPL) {
        // The writeback descriptor contains the just-completed descriptor's state
        // Its DESCADDR field points to the next descriptor that's now executing
        ::DmacDescriptor* wb = &dmac_writeback[id];

        // Only free descriptors if not in circular mode
        // In circular mode, descriptors are reused indefinitely
        if (!m_channels[id].isCircular()) {
            // Free the descriptor that just completed (if it's from our pool)
            // This is the descriptor we were tracking as "currently executing"
            if (m_currentExecutingDesc[id] != nullptr) {
                freeDescriptor(m_currentExecutingDesc[id]);
            }
        }

        // Update tracking: the next descriptor (from writeback's DESCADDR) is now executing
        // If DESCADDR is 0, we've reached the end and SUSP will fire next
        if (wb->DESCADDR.reg != 0) {
            m_currentExecutingDesc[id] = reinterpret_cast<DmacDescriptor*>(wb->DESCADDR.reg);
        } else {
            // End of chain - no more descriptors executing
            m_currentExecutingDesc[id] = nullptr;
        }

        Samd21::Dma::Reply reply;
        reply.set_status(Samd21::Dma::Status::OK);
        reply.set_remainingBytes(0);

        this->transactionIsrOut_out(id, reply);

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TCMPL;

        // Don't mark idle here - intermediate descriptors also trigger TCMPL
        // The channel is marked idle when end-of-chain is detected in the SUSP handler
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DmaDriver::sendTransactionIn_handler(FwIndexType portNum,
                                          const Samd21::Dma::TriggerSource& trigger,
                                          const Samd21::Dma::TransactionType& action,
                                          const Samd21::Dma::Priority& priority,
                                          U32 sourceAddr,
                                          U32 destAddr,
                                          U32 len,
                                          const Samd21::Dma::BeatSize& beatSize,
                                          bool incrementSource,
                                          bool incrementDestination,
                                          const Samd21::Dma::AddressIncrementStepSize& stepSize,
                                          const Samd21::Dma::StepSelection& stepSelection) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_SENDTRANSACTIONIN_INPUT_PORTS, portNum);

    // Search for a free DMAC Descriptor
    DmacDescriptor* desc = nullptr;
    for (U32 i = 0; i < DmaDriverConfig::DMA_DESCRIPTOR_N; i++) {
        if (!(this->m_descriptors_used & (1 << i))) {
            desc = &this->m_descriptors[i];
            setupDescriptor(desc, sourceAddr, destAddr, len, beatSize, incrementSource, incrementDestination, stepSize,
                            stepSelection);

            // Mark the descriptor as used
            this->m_descriptors_used |= (1 << i);
            break;
        }
    }

    // Make sure we got a descriptor
    FW_ASSERT(desc, portNum, static_cast<FwAssertArgType>(trigger.e));

    // Queue transaction on the channel
    bool wasIdle = !m_channels[portNum].isBusy();
    m_channels[portNum].queueTransaction(trigger, action, priority, desc);

    if (wasIdle) {
        // startTransaction() copied the descriptor to dmac_base[portNum]
        // The original can be freed, and dmac_base[portNum] is now "executing"
        freeDescriptor(desc);
        m_currentExecutingDesc[portNum] = reinterpret_cast<DmacDescriptor*>(&dmac_base[portNum]);
    }
    // else: appendToChain() linked desc by address - it will be freed when it completes
}

void DmaDriver::linkToFrontIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_LINKTOFRONTIN_INPUT_PORTS, portNum);

    m_channels[portNum].linkToFront();
}

Samd21::Dma::Writeback DmaDriver::popFrontIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_POPFRONTIN_INPUT_PORTS, portNum);

    Samd21::Dma::Writeback result;
    m_channels[portNum].popFront(result);
    return result;
}

void DmaDriver::suspendIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_SUSPENDIN_INPUT_PORTS, portNum);

    m_channels[portNum].suspend();
}

Samd21::Dma::Writeback DmaDriver::readWritebackIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_READWRITEBACKIN_INPUT_PORTS, portNum);

    // Read writeback descriptor for this channel
    ::DmacDescriptor* wb = &dmac_writeback[portNum];

    Samd21::Dma::Writeback result;
    result.set_btctrl(wb->BTCTRL.reg);
    result.set_btcnt(wb->BTCNT.reg);
    result.set_srcaddr(wb->SRCADDR.reg);
    result.set_dstaddr(wb->DSTADDR.reg);
    result.set_descaddr(wb->DESCADDR.reg);

    return result;
}

}  // namespace Samd21
