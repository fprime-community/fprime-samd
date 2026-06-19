// ======================================================================
// \title  DmaDriver.cpp
// \author tumbar
// \brief  cpp file for DmaDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/DmaDriver/DmaDriver.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "Os/Mutex.hpp"
#include "sam.h"

namespace Samd21 {

// Singleton instance for ISR access
DmaDriver* DmaDriver::s_instance = nullptr;

// External references to global descriptor memory (defined in DmaChannel.cpp)
extern DmacDescriptor dmac_base[DMAC_CH_NUM];
extern DmacDescriptor dmac_writeback[DMAC_CH_NUM];

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DmaDriver::DmaDriver(const char* const compName) : DmaDriverComponentBase(compName) {
    // Initialize each channel with its ID using setter
    for (U8 i = 0; i < DMAC_CH_NUM; i++) {
        m_channels[i].setChannelId(i);
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

// ----------------------------------------------------------------------
// Public member functions
// ----------------------------------------------------------------------

void DmaDriver::init() {
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
            DmacDescriptor* wb = &dmac_writeback[id];
            U32 beatSizeBytes = m_channels[id].getBeatSizeBytes();
            U32 remainingBytes = wb->BTCNT.reg * beatSizeBytes;

            Samd21::DmaReply reply;
            reply.set_status(Samd21::DmaStatus::BUS_ERROR);
            reply.set_remainingBytes(remainingBytes);

            this->transactionIsrOut_out(id, reply);
        }

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TERR;

        // Channel is disabled by hardware, mark as idle
        m_channels[id].markIdle();
    }

    // Handle Suspend
    if (flags & DMAC_CHINTFLAG_SUSP) {
        if (status & DMAC_CHSTATUS_FERR) {
            // Fetch error - check if end-of-chain or invalid descriptor
            DmacDescriptor* wb = &dmac_writeback[id];
            if (wb->DESCADDR.reg == 0x00000000) {
                // Normal end of linked list - treat as successful completion
                Samd21::DmaReply reply;
                reply.set_status(Samd21::DmaStatus::OK);
                reply.set_remainingBytes(0);

                this->transactionIsrOut_out(id, reply);
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

    // Handle Transfer Complete
    if (flags & DMAC_CHINTFLAG_TCMPL) {
        Samd21::DmaReply reply;
        reply.set_status(Samd21::DmaStatus::OK);
        reply.set_remainingBytes(0);

        this->transactionIsrOut_out(id, reply);

        // Clear flag
        DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TCMPL;

        m_channels[id].markIdle();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DmaDriver::sendTransactionIn_handler(FwIndexType portNum,
                                          const Samd21::DmaDriver_TriggerSource& trigger,
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
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_SENDTRANSACTIONIN_INPUT_PORTS, portNum);

    // Queue transaction on the channel (will start immediately if idle, or append if busy)
    m_channels[portNum].queueTransaction(trigger, action, priority, sourceAddr, destAddr, len, beatSize,
                                         incrementSource, incrementDestination, stepSize, stepSelection);
}

void DmaDriver::suspendIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_SUSPENDIN_INPUT_PORTS, portNum);

    m_channels[portNum].suspend();
}

Samd21::DmaWriteback DmaDriver::readWritebackIn_handler(FwIndexType portNum) {
    FW_ASSERT(m_initialized);
    FW_ASSERT(portNum < NUM_READWRITEBACKIN_INPUT_PORTS, portNum);

    // Read writeback descriptor for this channel
    DmacDescriptor* wb = &dmac_writeback[portNum];

    Samd21::DmaWriteback result;
    result.set_btctrl(wb->BTCTRL.reg);
    result.set_btcnt(wb->BTCNT.reg);
    result.set_srcaddr(wb->SRCADDR.reg);
    result.set_dstaddr(wb->DSTADDR.reg);
    result.set_descaddr(wb->DESCADDR.reg);

    return result;
}

}  // namespace Samd21
