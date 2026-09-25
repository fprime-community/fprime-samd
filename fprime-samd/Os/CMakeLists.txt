restrict_platforms(Samd21)
add_custom_target("${FPRIME_CURRENT_MODULE}")

# Add atomic operation stubs for Cortex-M0+
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Atomic")

register_os_implementation("Console" Samd21)

# Os::Mutex/Os::ConditionVariable are compile-time aliased directly to Samd21::Mutex and
# Os::Stub::Mutex::StubConditionVariable (see default/samd-config/OsDelegateMutex.hpp) rather than
# going through Os::DelegateMutex/Os::DelegateConditionVariable's runtime getDelegate() dispatch:
# baremetal SAMD21 has no threads, so there is never any contention to arbitrate at runtime, and
# link-time selection buys nothing here. register_os_implementation's Default${NAME}.cpp
# getDelegate() boilerplate would just be dead code, so this bundles the real classes directly and
# registers a bare pass-through implementation only to satisfy Os_Mutex's REQUIRES_IMPLEMENTATIONS.
register_fprime_module(
      Os_Mutex_Samd21_Implementation
    SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/Mutex.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/ConditionVariable.cpp"
    HEADERS
      "${CMAKE_CURRENT_LIST_DIR}/Mutex.hpp"
      "${CMAKE_CURRENT_LIST_DIR}/ConditionVariable.hpp"
    DEPENDS
      Fw_Time
      Fw_Types
      Os_Mutex
)
register_fprime_implementation(
    Os_Mutex_Samd21
  IMPLEMENTS
    Os_Mutex
  DEPENDS
    Os_Mutex_Samd21_Implementation
  INTERFACE
)
