restrict_platforms(Samd21)

register_fprime_implementation(
    Samd21_StringFormat_snprintf
  IMPLEMENTS
    Fw_StringFormat
  DEPENDS
    Fw_Types
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/snprintf_format.cpp"
)

register_fprime_implementation(
    Samd21_StringScan_sscanf
  IMPLEMENTS
    Fw_StringScan
  DEPENDS
    Fw_Types
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/sscanf_scan.cpp"
)

register_fprime_module(
    fprime-samd_Overrides_ResetAssertHook
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/ResetAssertHook.cpp"
  HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/ResetAssertHook.hpp"
  DEPENDS
    Fw_Types
    Fw_Com
)

if (SAMD21_LTO)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriver.cpp"
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )
endif()
