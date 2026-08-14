/**
 * atomic.c:
 *
 * GCC atomic builtin implementations for Cortex-M0+ which lacks native atomic instructions.
 *
 * These implementations disable interrupts during atomic operations to ensure atomicity
 * on single-core systems. This is safe for SAMD21 which is single-core.
 *
 * Uses CMSIS intrinsics (__get_PRIMASK, __disable_irq, __set_PRIMASK) which are properly
 * implemented with compiler barriers to prevent instruction reordering.
 *
 * NOTE: Function signatures use 'unsigned int' (not uint32_t) to match GCC's builtin
 * declarations exactly and avoid -Wbuiltin-declaration-mismatch warnings.
 */

#include <stdint.h>
#include <stdbool.h>
#include <sam.h>

/**
 * __atomic_compare_exchange_4:
 *
 * Atomically compares *ptr with *expected, and if equal, sets *ptr to desired.
 * Returns true if the exchange happened, false otherwise.
 * Always updates *expected to the old value of *ptr.
 */
bool __atomic_compare_exchange_4(volatile void* ptr,
                                 void* expected,
                                 unsigned int desired,
                                 bool weak,
                                 int success_memorder,
                                 int failure_memorder) {
    (void)weak;  // Unused for this implementation
    (void)success_memorder;
    (void)failure_memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    volatile unsigned int* ptr32 = (volatile unsigned int*)ptr;
    unsigned int* expected32 = (unsigned int*)expected;
    unsigned int old_value = *ptr32;
    bool success = (old_value == *expected32);

    if (success) {
        *ptr32 = desired;
    } else {
        // The spec is a little unclear on if expected should always be updated or only if success is false
        // this implementation is technically more efficient though
        *expected32 = old_value;
    }

    __set_PRIMASK(primask);

    return success;
}

/**
 * __atomic_load_4:
 *
 * Atomically load a 32-bit value.
 */
unsigned int __atomic_load_4(const volatile void* ptr, int memorder) {
    (void)memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    unsigned int value = *(const volatile unsigned int*)ptr;

    __set_PRIMASK(primask);

    return value;
}

/**
 * __atomic_store_4:
 *
 * Atomically store a 32-bit value.
 */
void __atomic_store_4(volatile void* ptr, unsigned int value, int memorder) {
    (void)memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    *(volatile unsigned int*)ptr = value;

    __set_PRIMASK(primask);
}

/**
 * __atomic_exchange_4:
 *
 * Atomically exchange a 32-bit value, returning the old value.
 */
unsigned int __atomic_exchange_4(volatile void* ptr, unsigned int value, int memorder) {
    (void)memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    volatile unsigned int* ptr32 = (volatile unsigned int*)ptr;
    unsigned int old_value = *ptr32;
    *ptr32 = value;

    __set_PRIMASK(primask);

    return old_value;
}

/**
 * __atomic_fetch_add_4:
 *
 * Atomically add to a 32-bit value, returning the old value.
 */
unsigned int __atomic_fetch_add_4(volatile void* ptr, unsigned int value, int memorder) {
    (void)memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    volatile unsigned int* ptr32 = (volatile unsigned int*)ptr;
    unsigned int old_value = *ptr32;
    *ptr32 = old_value + value;

    __set_PRIMASK(primask);

    return old_value;
}

/**
 * __atomic_fetch_sub_4:
 *
 * Atomically subtract from a 32-bit value, returning the old value.
 */
unsigned int __atomic_fetch_sub_4(volatile void* ptr, unsigned int value, int memorder) {
    (void)memorder;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    volatile unsigned int* ptr32 = (volatile unsigned int*)ptr;
    unsigned int old_value = *ptr32;
    *ptr32 = old_value - value;

    __set_PRIMASK(primask);

    return old_value;
}
