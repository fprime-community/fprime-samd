#ifndef FPRIME_SAMD_MCU_SYSTEMINIT_H
#define FPRIME_SAMD_MCU_SYSTEMINIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// System initialization function
void SystemInit(void);

// Global variable set by SystemInit (CMSIS standard)
extern uint32_t SystemCoreClock;

#ifdef __cplusplus
}
#endif

#endif // FPRIME_SAMD_MCU_SYSTEMINIT_H
