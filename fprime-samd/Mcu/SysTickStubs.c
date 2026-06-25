// ======================================================================
// \title  SysTickStubs.c
// \author Andrei Tumbar
// \brief  Stub implementations for SysTick functions (Arduino compatibility)
// ======================================================================

#include <stdint.h>

// Stub for sysTickHook - returns 0 to allow default handler to run
__attribute__((weak)) int sysTickHook(void) {
    return 0;
}

// Stub for SysTick_DefaultHandler - no-op
__attribute__((weak)) void SysTick_DefaultHandler(void) {
    // Default: do nothing
}
