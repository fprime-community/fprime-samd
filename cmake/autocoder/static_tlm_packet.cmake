####
# autocoder/static_tlm_packet.cmake:
#
# Runs `tools/static-tlm-packetizer` over a module's FPP files to generate telemetry
# packet code for each deployment topology it defines.
#
#     register_fprime_build_autocoder("autocoder/static_tlm_packet" OFF)
####
include_guard()
include(utilities)
include(autocoder/helpers)
include(autocoder/fpp)
include(autocoder/fpp_query)

autocoder_setup_for_multiple_sources()

get_filename_component(STATIC_TLM_PACKETIZER_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/static-tlm-packetizer" REALPATH)
set(STATIC_TLM_PACKETIZER "${STATIC_TLM_PACKETIZER_PATH}"
    CACHE INTERNAL "Path to static-tlm-packetizer" FORCE)

# The rules name the generated files, and both halves of the autocoder read them: the
# configure-time query below to declare the outputs, and the generator to name what it
# writes. One artifact, so the two cannot disagree about a path.
get_filename_component(STATIC_TLM_PACKET_RULES_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/static-tlm-packet.toml" REALPATH)
set(STATIC_TLM_PACKET_RULES "${STATIC_TLM_PACKET_RULES_PATH}"
    CACHE INTERNAL "Path to the static-tlm-packet fpp-query rules" FORCE)

####
# Function `static_tlm_packet_is_supported`:
#
# Processes FPP files. Which of them define a deployment topology is decided in
# `static_tlm_packet_setup_autocode` below.
#
# AC_INPUT_FILE: potential input to the autocoder
####
function(static_tlm_packet_is_supported AC_INPUT_FILE)
    autocoder_support_by_suffix(".fpp" "${AC_INPUT_FILE}" TRUE)
endfunction(static_tlm_packet_is_supported)

####
# Function `static_tlm_packet_setup_autocode`:
#
# Sets up the build step producing the telemetry packet source(s) for a module.
#
# MODULE_NAME: current module being processed
# AC_INPUT_FILES: list of supported autocoder input files
####
function(static_tlm_packet_setup_autocode MODULE_NAME AC_INPUT_FILES)
    fpp_query_filenames("static-tlm-packet" "${STATIC_TLM_PACKET_RULES}"
        "${AC_INPUT_FILES}" GENERATED_CPP)

    # Check if this module is actually generating any files
    if (NOT GENERATED_CPP)
        set(AUTOCODER_GENERATED_BUILD_SOURCES "" PARENT_SCOPE)
        return()
    else()
        set(AUTOCODER_GENERATED_BUILD_SOURCES "${GENERATED_CPP}" PARENT_SCOPE)
    endif()

    # F Prime CMake will provide us with FPP file dependencies. Only the generator
    # needs them: it runs the semantic analysis the query does without.
    fpp_info("${MODULE_NAME}" "${AC_INPUT_FILES}")
    fpp_autocoder_variables("${FPP_IMPORTS}")

    add_custom_command(
            OUTPUT ${GENERATED_CPP}
            COMMAND ${STATIC_TLM_PACKETIZER} "-d" "${CMAKE_CURRENT_BINARY_DIR}"
                "--rules" "${STATIC_TLM_PACKET_RULES}"
                ${FPP_IMPORT_FLAGS} ${AC_INPUT_FILES}
            DEPENDS ${FILE_DEPENDENCIES} "${STATIC_TLM_PACKETIZER}"
                "${STATIC_TLM_PACKET_RULES}"
            COMMENT "Generating telemetry packet code for ${MODULE_NAME}"
    )
endfunction(static_tlm_packet_setup_autocode)
