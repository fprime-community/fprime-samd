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
# getDelegate() boilerplate would just be dead code, so the implementation registers the real classes
# directly. They are the implementation's own SOURCES so they link into the deployment as object files: as an
# archive they sit ahead of libOs in the link order, and Os::Queue's static mutex (in libOs) would go unresolved
# unless some other always-linked object happened to reference Os::Samd21::Mutex first.
register_fprime_implementation(
    Os_Mutex_Samd21
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Mutex.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ConditionVariable.cpp"
  HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/Mutex.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ConditionVariable.hpp"
  IMPLEMENTS
    Os_Mutex
  DEPENDS
    Fw_Time
    Fw_Types
)
