####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

# Module names are derived from the path from the nearest project/library/framework
# root when not specifically overridden by the developer, i.e. the module defined by
# `MyProj/Some/Path/CMakeLists.txt` will be named `MyProj_Some_Path`.

# Select hardware implementation based on platform
if (FPRIME_PLATFORM STREQUAL "Samd21")
    # SAMD21 target build - use real hardware implementation
    set(SYSTEM_INFO_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/SystemInfoHardware.cpp")
else()
    # Linux/test build - use stub implementation
    set(SYSTEM_INFO_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/SystemInfoHardwareStub.cpp")
endif()

### Git commit stamp ###
# `SystemInfo` reports the HEAD commit of the project, fprime and fprime-samd over its
# SystemInfo event/channel. GitVersion.cmake generates a header holding those commits.
#
# The binary directory of this module already sits on the include path (fprime passes
# `-I${CMAKE_BINARY_DIR}`, the same mechanism that resolves the autocoded headers), so the
# generated header is included as "fprime-samd/Drv/SystemInfo/SystemInfoVersion.hpp".
find_package(Git QUIET)

# Resolve the fprime-samd root out of the library locations, falling back to this file's
# position within the library (<root>/fprime-samd/Drv/SystemInfo)
get_filename_component(SYSTEM_INFO_LIBRARY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
foreach (LIBRARY_LOCATION IN LISTS FPRIME_LIBRARY_LOCATIONS)
    get_filename_component(LIBRARY_NAME "${LIBRARY_LOCATION}" NAME)
    if (LIBRARY_NAME STREQUAL "fprime-samd")
        get_filename_component(SYSTEM_INFO_LIBRARY_ROOT "${LIBRARY_LOCATION}" ABSOLUTE)
        break()
    endif()
endforeach()

set(SYSTEM_INFO_VERSION_HPP "${CMAKE_CURRENT_BINARY_DIR}/SystemInfoVersion.hpp")
set(SYSTEM_INFO_VERSION_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/GitVersion.cmake")
set(SYSTEM_INFO_VERSION_ARGS
    "-DOUTPUT_FILE=${SYSTEM_INFO_VERSION_HPP}"
    "-DPROJECT_DIR=${FPRIME_PROJECT_ROOT}"
    "-DFPRIME_DIR=${FPRIME_FRAMEWORK_PATH}"
    "-DSAMD_DIR=${SYSTEM_INFO_LIBRARY_ROOT}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
)

# Generate at configure time so the header exists for the first compile and for tooling
# reading compile_commands.json
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${SYSTEM_INFO_VERSION_ARGS} -P "${SYSTEM_INFO_VERSION_SCRIPT}"
    RESULT_VARIABLE SYSTEM_INFO_VERSION_RESULT
)
if (NOT SYSTEM_INFO_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR "[SystemInfo] Failed to generate ${SYSTEM_INFO_VERSION_HPP}")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfo.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfo.cpp"
        ${SYSTEM_INFO_HARDWARE_SOURCE}
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfo.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfoHardware.hpp"
        "${SYSTEM_INFO_VERSION_HPP}"
    DEPENDS
        fprime-samd_Drv_Types
)

# Re-stamp on every build so a new commit is picked up without re-configuring. The script
# leaves the header untouched when the commits have not moved, so this does not force a
# rebuild; when they have, the compiler's header dependency scanning rebuilds SystemInfo.cpp.
add_custom_target("${FPRIME_CURRENT_MODULE}_version"
    COMMAND "${CMAKE_COMMAND}" ${SYSTEM_INFO_VERSION_ARGS} -P "${SYSTEM_INFO_VERSION_SCRIPT}"
    BYPRODUCTS "${SYSTEM_INFO_VERSION_HPP}"
    COMMENT "Stamping git commits into ${FPRIME_CURRENT_MODULE} SystemInfoVersion.hpp"
    VERBATIM
)
add_dependencies("${FPRIME_CURRENT_MODULE}" "${FPRIME_CURRENT_MODULE}_version")

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfo.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SystemInfoTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SystemInfoTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/SystemInfoHardwareStub.cpp"
    DEPENDS
        STest # For rules-based testing
    UT_AUTO_HELPERS
)

# The unit tests compare the reported commits against SystemInfoVersion.hpp, so the UT
# executable needs the stamp to be current too
if (TARGET "${FPRIME_CURRENT_MODULE}_ut_exe")
    add_dependencies("${FPRIME_CURRENT_MODULE}_ut_exe" "${FPRIME_CURRENT_MODULE}_version")
endif()
