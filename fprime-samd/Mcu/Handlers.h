#ifndef FPRIME_SAMD_MCU_HANDLERS_H
#define FPRIME_SAMD_MCU_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

// Cortex-M interrupt handlers and vector table

// RTOS hook functions (weak, can be overridden)
extern void svcHook(void);
extern void pendSVHook(void);
extern int sysTickHook(void);

// USB ISR registration
void USB_SetHandler(void (*new_usb_isr)(void));

#ifdef __cplusplus
}
#endif

#endif // FPRIME_SAMD_MCU_HANDLERS_H
