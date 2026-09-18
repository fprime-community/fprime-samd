####
# autocoder/static_cmd_dispatch.cmake:
#
# Runs `tools/static-cmd-dispatcher` over a module's FPP files to generate command
# dispatch code for each deployment topology it defines.
#
#     register_fprime_build_autocoder("autocoder/static_cmd_dispatch" OFF)
####
include_guard()
include(utilities)
include(autocoder/helpers)
include(autocoder/fpp)

autocoder_setup_for_multiple_sources()

get_filename_component(STATIC_CMD_DISPATCHER_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/static-cmd-dispatcher" REALPATH)
set(STATIC_CMD_DISPATCHER "${STATIC_CMD_DISPATCHER_PATH}"
    CACHE INTERNAL "Path to static-cmd-dispatcher" FORCE)

####
# Function `static_cmd_dispatch_is_supported`:
#
# Processes FPP files. Which of them define a deployment topology is decided in
# `static_cmd_dispatch_setup_autocode` below.
#
# AC_INPUT_FILE: potential input to the autocoder
####
function(static_cmd_dispatch_is_supported AC_INPUT_FILE)
    autocoder_support_by_suffix(".fpp" "${AC_INPUT_FILE}" TRUE)
endfunction(static_cmd_dispatch_is_supported)

####
# Function `static_cmd_dispatch_setup_autocode`:
#
# Sets up the build step producing the command dispatch source(s) for a module.
#
# MODULE_NAME: current module being processed
# AC_INPUT_FILES: list of supported autocoder input files
####
function(static_cmd_dispatch_setup_autocode MODULE_NAME AC_INPUT_FILES)
    # F Prime CMake will provide us with FPP file dependencies
    fpp_info("${MODULE_NAME}" "${AC_INPUT_FILES}")
    fpp_autocoder_variables("${FPP_IMPORTS}")
    set(STATIC_CMD_DISPATCH_FILENAMES "${CMAKE_CURRENT_BINARY_DIR}/static-cmd-dispatch-cache-filenames.txt")
    execute_process_or_fail(
        "[static_cmd_dispatch] could not list generated files for ${MODULE_NAME}"
        "${STATIC_CMD_DISPATCHER}"
        "--filenames" "${STATIC_CMD_DISPATCH_FILENAMES}"
        "-d" "${CMAKE_CURRENT_BINARY_DIR}"
        ${AC_INPUT_FILES}
    )
    file(STRINGS "${STATIC_CMD_DISPATCH_FILENAMES}" GENERATED_CPP)

    # Check if this module is actually generating any files
    if (NOT GENERATED_CPP)
        set(AUTOCODER_GENERATED_BUILD_SOURCES "" PARENT_SCOPE)
        return()
    else()
        set(AUTOCODER_GENERATED_BUILD_SOURCES "${GENERATED_CPP}" PARENT_SCOPE)
    endif()

    add_custom_command(
            OUTPUT ${GENERATED_CPP}
            COMMAND ${STATIC_CMD_DISPATCHER} "-d" "${CMAKE_CURRENT_BINARY_DIR}"
                ${FPP_IMPORT_FLAGS} ${AC_INPUT_FILES}
            DEPENDS ${FILE_DEPENDENCIES} "${STATIC_CMD_DISPATCHER}"
            COMMENT "Generating command dispatch code for ${MODULE_NAME}"
    )
endfunction(static_cmd_dispatch_setup_autocode)
