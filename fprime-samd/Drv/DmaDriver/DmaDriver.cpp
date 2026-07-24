// ======================================================================
// \title  DmaDriver.cpp
// \author tumbar
// \brief  cpp file for DmaDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/DmaDriver/DmaDriver.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "config-samd/DmaDriverConfig.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwIndexTypeAliasAc.h"
#include "fprime-samd/Drv/DmaDriver/DmaChannel.hpp"
#include "fprime-samd/Drv/Types/AddressIncrementStepSizeEnumAc.hpp"
#include "fprime-samd/Drv/Types/BeatSizeEnumAc.hpp"
#include "fprime-samd/Drv/Types/CriticalSection.hpp"
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

DmaDriver::DmaDriver(const char* const compName)
    : DmaDriverComponentBase(compName), m_descriptors(), m_descriptors_used(0), m_initialized(false) {
    // Initialize each channel with its ID using setter
    for (U8 i = 0; i < DMAC_CH_NUM; i++) {
        m_channels[i].setChannelId(i);
        m_currentExecutingDesc[i] = nullptr;
    }

    // Register singleton for ISR access
    FW_ASSERT(s_instance == nullptr);
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

static U32 calculateEndAddress(U32 startAddr,
                               U16 btcnt,
                               Samd21::Dma::BeatSize beatSize,
                               Samd21::Dma::AddressIncrementStepSize stepSize,
                               bool increment) {
    if (increment) {
        return startAddr + btcnt * (beatSize.e + 1) * (1 << stepSize.e);
    } else {
        return startAddr + btcnt * (beatSize.e + 1);
    }
}

static void setupDescriptor(DmacDescriptor* desc,
                            U32 sourceAddr,
                            U32 destAddr,
                            U16 btcnt,
                            const Samd21::Dma::BeatSize& beatSize,
                            bool incrementSource,
                            bool incrementDestination,
                            const Samd21::Dma::AddressIncrementStepSize& stepSize,
                            const Samd21::Dma::StepSelection& stepSelection) {
    static_assert(sizeof(::DmacDescriptor) == sizeof(Samd21::DmacDescriptor),
                  "sam.h descriptor does not match Samd21::Dm");

    // Validate parameters
    FW_ASSERT(desc != nullptr);
    FW_ASSERT(sourceAddr != 0);
    FW_ASSERT(destAddr != 0);

    // Cast to hardware descriptor type (layout verified by static_assert at line 77)
    auto* desc_hw = reinterpret_cast<::DmacDescriptor*>(desc);

    // Setup BTCTRL - Block Transfer Control
    desc_hw->BTCTRL.bit.VALID = 1;
    desc_hw->BTCTRL.bit.BLOCKACT = DMAC_BTCTRL_BLOCKACT_INT_Val;
    desc_hw->BTCTRL.bit.BEATSIZE = beatSize.e;
    desc_hw->BTCTRL.bit.SRCINC = incrementSource ? 1 : 0;
    desc_hw->BTCTRL.bit.DSTINC = incrementDestination ? 1 : 0;
    desc_hw->BTCTRL.bit.STEPSEL = stepSelection.e;
    desc_hw->BTCTRL.bit.STEPSIZE = stepSize.e;

    // Setup beat count
    desc_hw->BTCNT.reg = btcnt;

    // 20.6.2.7. Addressing
    // When source address incrementation is configured (BTCTRL.SRCINC=1), SRCADDR is calculated as follows:
    // If BTCTRL.STEPSEL=1:
    //      SRCADDR = SRCADDRSTART + BTCNT ⋅ (BEATSIZE + 1) ⋅ 2**STEPSIZE
    // If BTCTRL.STEPSEL=0:
    //      SRCADDR = SRCADDRSTART + BTCNT ⋅ (BEATSIZE + 1)
    desc_hw->SRCADDR.reg = calculateEndAddress(sourceAddr, incrementSource ? btcnt : 0, beatSize, stepSize,
                                               incrementSource && stepSelection == Dma::StepSelection::SOURCE);
    desc_hw->DSTADDR.reg =
        calculateEndAddress(destAddr, incrementDestination ? btcnt : 0, beatSize, stepSize,
                            incrementDestination && stepSelection == Dma::StepSelection::DESTINATION);

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
    this->m_descriptors_used &= ~(1U << index);
}

void DmaDriver::freeChain(DmacDescriptor* chainStart) {
    auto current = reinterpret_cast<::DmacDescriptor*>(chainStart);

    U32 bound = 0;
    while (bound <= DmaDriverConfig::DMA_DESCRIPTOR_N && current != nullptr && current->DESCADDR.reg != 0) {
        // Get next descriptor before freeing current
        auto next = reinterpret_cast<::DmacDescriptor*>(current->DESCADDR.reg);

        // Free current descriptor if it's from our pool
        freeDescriptor(reinterpret_cast<DmacDescriptor*>(current));

        current = next;
        bound++;
    }

    // Make sure we didn't hit the descriptor bound
    FW_ASSERT(bound <= DmaDriverConfig::DMA_DESCRIPTOR_N);

    // Free the last descriptor (with DESCADDR == 0)
    if (current != nullptr) {
        freeDescriptor(reinterpret_cast<DmacDescriptor*>(current));
    }
}

void DmaDriver::configure() {
    FW_ASSERT(!m_initialized);

    // Enable DMAC clocks
    PM->AHBMASK.reg |= PM_AHBMASK_DMAC;
    PM->APBBMASK.reg |= PM_APBBMASK_DMAC;

    // Perform software reset
    DMAC->CTRL.bit.DMAENABLE = 0;
    DMAC->CTRL.bit.SWRST = 1;
    waitForDmacReset();

    // Keep the DMAC running during debug pause
    DMAC->DBGCTRL.bit.DBGRUN = 1;

    // Setup descriptor base address and write-back section base address - hardware requires pointer as uint32_t
    DMAC->BASEADDR.reg = reinterpret_cast<U32>(dmac_base);
    DMAC->WRBADDR.reg = reinterpret_cast<U32>(dmac_writeback);

    // Clear descriptor memory
    memset(dmac_base, 0, sizeof(dmac_base));
    memset(dmac_writeback, 0, sizeof(dmac_writeback));

    // Enable DMA controller with all priority levels
    DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);

    // Enable DMA interrupt at lowest priority
    static constexpr U32 LOWEST_PRIORITY = (1U << __NVIC_PRIO_BITS) - 1;
    NVIC_EnableIRQ(DMAC_IRQn);
    NVIC_SetPriority(DMAC_IRQn, LOWEST_PRIORITY);

    m_initialized = true;
}

void DmaDriver::handleInterrupt() {
    Samd21::CriticalSection::enter();

    // Read which channel triggered the interrupt
    U8 id = DMAC->INTPEND.bit.ID;
    FW_ASSERT(id < DMAC_CH_NUM, id);

    // Select the channel
    DMAC->CHID.reg = DMAC_CHID_ID(id);
    U8 flags = DMAC->CHINTFLAG.reg;
    U8 status = DMAC->CHSTATUS.reg;

    // Handle Transfer Error (bus error)
    if (flags & DMAC_CHINTFLAG_TERR) {
        // Check if also invalid descriptor (programming error)
        if (status & DMAC_CHSTATUS_FERR) {
            // Invalid descriptor - should never happen
            FW_ASSERT(false, id);
        } else {
            // Pure bus error - report it
            ::DmacDescriptor* wb = &dmac_writeback[id];

            Samd21::Dma::Reply reply;
            reply.set_status(Samd21::Dma::Status::BUS_ERROR);
            reply.set_remainingBytes(wb->BTCNT.reg);

            this->transactionIsrOut_out(id, reply);
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TERR;

        // Channel is disabled by hardware on error. The whole queued chain is
        // abandoned: walk from the anchor freeing every linked pool slot.
        if (!m_channels[id].isCircular()) {
            ::DmacDescriptor* cur = reinterpret_cast<::DmacDescriptor*>(m_currentExecutingDesc[id]);
            U32 bound = 0;
            while (cur != nullptr && bound <= DmaDriverConfig::DMA_DESCRIPTOR_N) {
                ::DmacDescriptor* next = reinterpret_cast<::DmacDescriptor*>(cur->DESCADDR.reg);
                freeDescriptor(reinterpret_cast<DmacDescriptor*>(cur));
                cur = next;
                bound++;
            }
            FW_ASSERT(bound <= DmaDriverConfig::DMA_DESCRIPTOR_N, id);

            // Detach the freed chain from the anchor.
            dmac_base[id].DESCADDR.reg = 0;
        }

        m_currentExecutingDesc[id] = nullptr;
        m_channels[id].markIdle();
    }

    // Handle Transfer Complete. Must run before SUSP: at end-of-chain both TCMPL
    // and the fetch-error SUSP can be pending together, and servicing SUSP first
    // would mark the channel idle and leak the final descriptor.
    if (flags & DMAC_CHINTFLAG_TCMPL) {
        // A single TCMPL can coalesce several completed blocks, so reply+free
        // every descriptor completed since we last ran, in submission order.
        ::DmacDescriptor* wb = &dmac_writeback[id];

        if (!m_channels[id].isCircular()) {
            // Find the boundary: the "keep" node the DMAC is currently on. Every
            // node before it has completed; it and everything after have not.
            //
            // The write-back holds one of two states (datasheet 21.6.2.5): after a
            // block completes, its descriptor is written back with VALID=0, then
            // the DMAC pre-fetches the NEXT descriptor into the write-back with
            // VALID=1 before the arbiter re-runs. Because this ISR is lowest
            // priority it may observe either:
            //   VALID==0: wb mirrors the just-completed node; wb->DESCADDR is the
            //             ADDRESS of the node now executing (0 => ran off the end,
            //             chain fully drained).
            //   VALID==1: wb mirrors the now-executing node; wb->DESCADDR is the
            //             address of the node AFTER it, so the executing node is
            //             the one in our chain whose own DESCADDR == wb->DESCADDR.
            // In both states the "keep" frontier is the node the DMAC is on now,
            // regardless of how many blocks coalesced into this one interrupt.
            U32 wbLink = wb->DESCADDR.reg;
            bool wbValid = (wb->BTCTRL.bit.VALID != 0);

            // Fully drained only when the completed node's link was terminal
            // (VALID==0 && DESCADDR==0). A VALID==1 write-back with DESCADDR==0 is
            // the tail node still executing -- not drained.
            bool drained = (!wbValid && wbLink == 0);

            ::DmacDescriptor* keep = nullptr;  // boundary node to retain (null => drained)
            if (!drained) {
                ::DmacDescriptor* cur = reinterpret_cast<::DmacDescriptor*>(m_currentExecutingDesc[id]);
                U32 bound = 0;
                while (cur != nullptr && bound <= DmaDriverConfig::DMA_DESCRIPTOR_N) {
                    // VALID==1: match the executing node by its own link.
                    // VALID==0: match the executing node by its address.
                    U32 probe = wbValid ? cur->DESCADDR.reg : reinterpret_cast<U32>(cur);
                    if (probe == wbLink) {
                        keep = cur;
                        break;
                    }
                    cur = reinterpret_cast<::DmacDescriptor*>(cur->DESCADDR.reg);
                    bound++;
                }
                FW_ASSERT(bound <= DmaDriverConfig::DMA_DESCRIPTOR_N, id);
                FW_ASSERT(keep != nullptr, id);
            }

            // Reply+free every completed node up to (not including) the boundary.
            ::DmacDescriptor* cur = reinterpret_cast<::DmacDescriptor*>(m_currentExecutingDesc[id]);
            U32 bound = 0;
            while (cur != nullptr && cur != keep && bound <= DmaDriverConfig::DMA_DESCRIPTOR_N) {
                ::DmacDescriptor* next = reinterpret_cast<::DmacDescriptor*>(cur->DESCADDR.reg);

                freeDescriptor(reinterpret_cast<DmacDescriptor*>(cur));

                Samd21::Dma::Reply reply;
                reply.set_status(Samd21::Dma::Status::OK);
                reply.set_remainingBytes(0);
                this->transactionIsrOut_out(id, reply);

                cur = next;
                bound++;
            }
            FW_ASSERT(bound <= DmaDriverConfig::DMA_DESCRIPTOR_N, id);

            if (keep != nullptr) {
                // Relink the anchor past the freed nodes so appendToChain never
                // traverses a reclaimed slot; the boundary is the new oldest node.
                dmac_base[id].DESCADDR.reg = reinterpret_cast<U32>(keep);
                m_currentExecutingDesc[id] = reinterpret_cast<DmacDescriptor*>(keep);
            } else {
                // Whole chain drained.
                dmac_base[id].DESCADDR.reg = 0;
                m_currentExecutingDesc[id] = nullptr;
                m_channels[id].markIdle();
            }
        } else {
            // Emit one reply per completed block.
            Samd21::Dma::Reply reply;
            reply.set_status(Samd21::Dma::Status::OK);
            reply.set_remainingBytes(0);
            this->transactionIsrOut_out(id, reply);
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TCMPL;
    }

    // Handle Suspend
    if (flags & DMAC_CHINTFLAG_SUSP) {
        if (status & DMAC_CHSTATUS_FERR) {
            // Fetch error - check if end-of-chain or invalid descriptor
            ::DmacDescriptor* wb = &dmac_writeback[id];
            if (wb->DESCADDR.reg == 0) {
                // Normal end of linked list. The completed descriptor was already
                // freed by the TCMPL handler above; just mark the channel idle.
                m_currentExecutingDesc[id] = nullptr;
                m_channels[id].markIdle();
            } else {
                // Invalid descriptor with non-zero DESCADDR - programming error
                FW_ASSERT(false, id, wb->DESCADDR.reg);
            }
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_SUSP;
    }

    Samd21::CriticalSection::leave();
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DmaDriver ::abortTransactionIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_ABORTTRANSACTIONIN_INPUT_PORTS, portNum);

    Samd21::CriticalSection cs;

    // Nothing to abort on an idle channel.
    if (!m_channels[portNum].isBusy()) {
        return;
    }

    // Disable the channel: the active beat finishes, the write-back captures its
    // final progress, and all queued descriptors are dropped. No DMAC interrupt is
    // raised, so we free the chain.
    U16 remainingBeats = 0;
    m_channels[portNum].abort(remainingBeats);

    // Free every descriptor still linked from the anchor (active + dropped chain).
    ::DmacDescriptor* cur = reinterpret_cast<::DmacDescriptor*>(m_currentExecutingDesc[portNum]);
    U32 bound = 0;
    while (cur != nullptr && bound <= DmaDriverConfig::DMA_DESCRIPTOR_N) {
        ::DmacDescriptor* next = reinterpret_cast<::DmacDescriptor*>(cur->DESCADDR.reg);
        freeDescriptor(reinterpret_cast<DmacDescriptor*>(cur));
        cur = next;
        bound++;
    }

    FW_ASSERT(bound <= DmaDriverConfig::DMA_DESCRIPTOR_N, portNum);

    // Detach the freed chain from the anchor.
    dmac_base[portNum].DESCADDR.reg = 0;
    m_currentExecutingDesc[portNum] = nullptr;
}

void DmaDriver::sendTransactionIn_handler(FwIndexType portNum,
                                          const Samd21::Dma::TriggerSource& trigger,
                                          const Samd21::Dma::TransactionType& action,
                                          const Samd21::Dma::Priority& priority,
                                          U32 sourceAddr,
                                          U32 destAddr,
                                          U16 btcnt,
                                          const Samd21::Dma::BeatSize& beatSize,
                                          bool incrementSource,
                                          bool incrementDestination,
                                          const Samd21::Dma::AddressIncrementStepSize& stepSize,
                                          const Samd21::Dma::StepSelection& stepSelection) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_SENDTRANSACTIONIN_INPUT_PORTS, portNum);

    // Search for a free DMAC Descriptor
    DmacDescriptor* desc = nullptr;
    U32 i = 0;
    for (; i < DmaDriverConfig::DMA_DESCRIPTOR_N; i++) {
        if (!(this->m_descriptors_used & (1 << i))) {
            desc = &this->m_descriptors[i];
            setupDescriptor(desc, sourceAddr, destAddr, btcnt, beatSize, incrementSource, incrementDestination,
                            stepSize, stepSelection);

            // Mark the descriptor as used
            this->m_descriptors_used |= (1 << i);
            break;
        }
    }

    // Make sure we got a descriptor
    FW_ASSERT(desc, portNum, static_cast<FwAssertArgType>(trigger.e));

    Samd21::CriticalSection::enter();

    // Queue transaction on the channel
    bool wasIdle = !m_channels[portNum].isBusy();

    m_channels[portNum].queueTransaction(trigger, action, priority, desc);

    // The descriptor was copied into dmac_base so we no longer need the one we allocated
    if (wasIdle) {
        freeDescriptor(desc);
        m_currentExecutingDesc[portNum] = reinterpret_cast<DmacDescriptor*>(&dmac_base[portNum]);
    }

    Samd21::CriticalSection::leave();
}

void DmaDriver::linkToFrontIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_LINKTOFRONTIN_INPUT_PORTS, portNum);

    Samd21::CriticalSection::enter();

    m_channels[portNum].linkToFront();

    Samd21::CriticalSection::leave();
}

Samd21::Dma::Writeback DmaDriver::readWritebackIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_READWRITEBACKIN_INPUT_PORTS, portNum);

    Samd21::CriticalSection::enter();

    Samd21::Dma::Writeback result;
    m_channels[portNum].readWriteback(result);

    Samd21::CriticalSection::leave();

    return result;
}

}  // namespace Samd21
