####
# arm-m0/Platform/CMakeLists.txt:
#
# This file defines a configuration module for arm platforms.
####
register_fprime_config(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PlatformTypes.fpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/PlatformTypes.h"
    CHOOSES_IMPLEMENTATIONS
        Os_File_Stub
        Os_Generic_PriorityQueue
        Os_Mutex_Samd21
        Os_Cpu_Baremetal
        Os_Memory_Baremetal
        Os_Task_Baremetal
        Os_Console_Samd21
        Os_RawTime_Samd21
        Samd21_StringFormat_snprintf
        Samd21_StringScan_sscanf
        Samd21_AssertHook_reset
    INTERFACE # No buildable files generated
)
