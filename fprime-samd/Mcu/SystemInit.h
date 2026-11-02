// ======================================================================
// \title  SystemInit.h
// \author tumbar
// \brief  SAMD21 system clock initialization header
// ======================================================================

#ifndef FPRIME_SAMD_SYSTEMINIT_H
#define FPRIME_SAMD_SYSTEMINIT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Initialize the SAMD21 system clocks
 *
 * Configures the SAMD21 to run at 48MHz using the DFLL48M.
 * This function must be called before any peripheral initialization.
 *
 * Clock configuration:
 * - GCLK0 (main clock): DFLL48M @ 48MHz
 * - GCLK1: XOSC32K or OSC32K @ 32.768kHz
 * - GCLK3: OSC8M @ 8MHz
 * - Flash wait states: 1 (for 48MHz operation)
 * - CPU/APB dividers: all set to /1
 */
void SystemInit(void);

/**
 * \brief Set to 1 by every interrupt entry of the SAMD21 vector table (Mcu/reset.c)
 *
 * Samd21::Idle (Drv/Types) clears it before a Svc::PassiveCycler pass and refuses to
 * sleep while it is set, so an interrupt taken during the pass causes one more pass
 * instead of a sleep that lasts until the next interrupt.
 */
extern volatile uint32_t samd21_irq_taken;

#ifdef __cplusplus
}
#endif

#endif  // FPRIME_SAMD_SYSTEMINIT_H
