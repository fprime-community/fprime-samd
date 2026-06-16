// ======================================================================
// \title  DmaChannel.cpp
// \author tumbar
// \brief  cpp fifile for DmaChannel helper class
// ======================================================================

#include "Samd21/Drv/DmaDriver/DmaChannel.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "cmsis_gcc.h"

#include "sam.h"

namespace Samd21 {

__attribute__((__aligned__(16))) static DmacDescriptor    ///< 128 bit alignment
    dmac_base[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR,       ///< Descriptor table
    dmac_writeback[DMAC_CH_NUM] SECTION_DMAC_DESCRIPTOR;  ///< Writeback table

// Adapted from ASF3 interrupt_sam_nvic.c:

static volatile unsigned long cpu_irq_critical_section_counter = 0;
static volatile unsigned char cpu_irq_prev_interrupt_state = 0;

static void cpu_irq_enter_critical(void) {
    if (!cpu_irq_critical_section_counter) {
        if (__get_PRIMASK() == 0) {  // IRQ enabled?
            __disable_irq();         // Disable it
            __DMB();
            cpu_irq_prev_interrupt_state = 1;
        } else {
            // Make sure the to save the prev state as false
            cpu_irq_prev_interrupt_state = 0;
        }
    }

    cpu_irq_critical_section_counter++;
}

static void cpu_irq_leave_critical(void) {
    // Check if the user is trying to leave a critical section
    // when not in a critical section
    if (cpu_irq_critical_section_counter > 0) {
        cpu_irq_critical_section_counter--;

        // Only enable global interrupts when the counter
        // reaches 0 and the state of the global interrupt flag
        // was enabled when entering critical state */
        if ((!cpu_irq_critical_section_counter) && cpu_irq_prev_interrupt_state) {
            __DMB();
            __enable_irq();
        }
    }
}

DmaChannel::DmaChannel(U8 channel_id) : m_channel_id(channel_id), m_configured(false), m_busy(false) {}

void DmaChannel::configure(const Samd21::DmaDriver_TriggerSource& trigger,
                           const Samd21::DmaDriver_TransactionType& action,
                           const Samd21::DmaDriver_BeatSize& beatSize,
                           const Samd21::DmaDriver_Priority& priority,
                           bool incrementSource,
                           bool incrementDestination,
                           const Samd21::DmaDriver_AddressIncrementStepSize& stepSize,
                           const Samd21::DmaDriver_StepSelection& stepSelection) {
    FW_ASSERT(!m_busy, m_channel_id);
    FW_ASSERT(trigger.isValid(), m_channel_id, trigger.e);
    FW_ASSERT(action.isValid(), m_channel_id, action.e);
    FW_ASSERT(beatSize.isValid(), m_channel_id, beatSize.e);
    FW_ASSERT(priority.isValid(), m_channel_id, priority.e);
    FW_ASSERT(stepSize.isValid(), m_channel_id, stepSize.e);
    FW_ASSERT(stepSelection.isValid(), m_channel_id, stepSelection.e);

    cpu_irq_enter_critical();

    PM->AHBMASK.bit.DMAC_ = 1;  // Initialize DMA clocks
    PM->APBBMASK.bit.DMAC_ = 1;
    DMAC->CTRL.bit.DMAENABLE = 0;  // Disable DMA controller
    DMAC->CTRL.bit.SWRST = 1;      // Perform software reset

    // dmac_base[0].DESCADDR.reg = static_cast<U32>();

    // Initialize descriptor list addresses
    DMAC->BASEADDR.bit.BASEADDR = (uint32_t)dmac_base;
    DMAC->WRBADDR.bit.WRBADDR = (uint32_t)dmac_writeback;
    memset(dmac_base, 0, sizeof(dmac_base));
    memset(dmac_writeback, 0, sizeof(dmac_writeback));

    // Re-enable DMA controller with all priority levels
    DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);

    // Enable DMA interrupt at lowest priority
    NVIC_EnableIRQ(DMAC_IRQn);
    NVIC_SetPriority(DMAC_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

    m_configured = true;

    // Reset the allocated channel
    DMAC->CHID.bit.ID = m_channel_id;
    DMAC->CHCTRLA.bit.ENABLE = 0;
    DMAC->CHCTRLA.bit.SWRST = 1;

    // Clear software trigger
    DMAC->SWTRIGCTRL.reg &= ~(1 << m_channel_id);

    // Set the priority level
    DMAC->CHCTRLB.bit.LVL = static_cast<U8>(priority.e);

    // Set the trigger source
    DMAC->CHCTRLB.bit.TRIGSRC = static_cast<U8>(trigger.e);

    // Set the trigger action
    DMAC->CHCTRLB.bit.TRIGACT = static_cast<U8>(action.e);

    cpu_irq_leave_critical();
}
}  // namespace Samd21
