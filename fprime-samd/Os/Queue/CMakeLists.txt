####
# fprime-samd/Os/Queue/CMakeLists.txt:
#
# Statically allocated FIFO Os::Queue. Not restricted to the Samd21 platform so the unit tests can run natively
# (fprime-samd_Drv_Types supplies a stub CriticalSection there).
####
register_fprime_module(
      Os_Queue_Samd21_Implementation
    SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/Queue.cpp"
    HEADERS
      "${CMAKE_CURRENT_LIST_DIR}/Queue.hpp"
    DEPENDS
      Fw_Types
      fprime-samd_Drv_Types
      fprime-samd_Svc_StaticMallocator
      config-fprime-samd
)
register_fprime_implementation(
    Os_Queue_Samd21
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/DefaultQueue.cpp"
  IMPLEMENTS
    Os_Queue
  DEPENDS
    Fw_Types
    Os_Queue_Samd21_Implementation
)

register_fprime_ut(
    Samd21QueueTest
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/test/ut/Samd21QueueTests.cpp"
  DEPENDS
    Fw_Types
    Os
    config-fprime-samd
  CHOOSES_IMPLEMENTATIONS
    Os_Queue_Samd21
)
