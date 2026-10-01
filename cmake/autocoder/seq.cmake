####
# autocoder/seq.cmake:
#
# Compiles `.seq` files passed as AUTOCODER_INPUTS into linkable
# `Samd21::Seq::Action` record bodies:
#
#   1. `fprime-seqgen` assembles the `.seq` against this module's own topology
#      dictionary into a `.bin` (with the seqgen header + CRC footer).
#   2. `seq_to_c.py strip` drops that header/footer and emits a `.c`/`.h` pair
#      defining `fprime_seq_<NAME>[]` / `fprime_seq_<NAME>_len` (C linkage).
#
# The generated `.c` files are added directly to the module's build sources,
# so no separate library target is needed.
#
# A `.seq` file must be passed as an AUTOCODER_INPUT to the topology module
# itself (the one whose CMakeLists.txt defines topology.fpp/instances.fpp),
# since that module's own FPRIME_DICTIONARIES property is what's queried.
#
#     register_fprime_build_autocoder("autocoder/seq" OFF)
####
include_guard()
include(utilities)
include(autocoder/helpers)

autocoder_setup_for_multiple_sources()

get_filename_component(SEQ_TO_C
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/seq_to_c.py" REALPATH)

####
# Function `seq_is_supported`:
#
# AC_INPUT_FILE: potential input to the autocoder
####
function(seq_is_supported AC_INPUT_FILE)
    autocoder_support_by_suffix(".seq" "${AC_INPUT_FILE}" TRUE)
endfunction(seq_is_supported)

####
# Function `seq_setup_autocode`:
#
# Sets up the build step producing a `.c`/`.h` pair for each `.seq` input file.
#
# MODULE_NAME: current module being processed (the topology module)
# AC_INPUT_FILES: list of supported autocoder input files
####
function(seq_setup_autocode MODULE_NAME AC_INPUT_FILES)
    # This autocoder runs while MODULE_NAME's own build target is still being assembled,
    # before fprime_attach_custom_targets() creates "${MODULE_NAME}_dictionary" and sets
    # FPRIME_DICTIONARIES on MODULE_NAME. A plain get_target_property() would read that
    # property before it exists, so defer the read to build time via a generator
    # expression instead. Referencing MODULE_NAME's own property this way does not
    # introduce a new Ninja dependency edge beyond the target itself, so (unlike
    # referencing a sibling module) it cannot create a cycle.
    set(SEQS_DICTIONARY "$<TARGET_PROPERTY:${MODULE_NAME},FPRIME_DICTIONARIES>")

    set(GENERATED_SOURCES)
    foreach(SEQ_FILE IN LISTS AC_INPUT_FILES)
        get_filename_component(SEQ_NAME "${SEQ_FILE}" NAME_WE)
        set(SEQ_BIN "${CMAKE_CURRENT_BINARY_DIR}/${SEQ_NAME}.bin")
        set(SEQ_C "${CMAKE_CURRENT_BINARY_DIR}/${SEQ_NAME}.c")
        set(SEQ_H "${CMAKE_CURRENT_BINARY_DIR}/${SEQ_NAME}.h")

        add_custom_command(
            OUTPUT "${SEQ_C}" "${SEQ_H}"
            COMMAND "${PYTHON}" -m fprime_gds.common.tools.seqgen
                    --dictionary "${SEQS_DICTIONARY}"
                    "${SEQ_FILE}" "${SEQ_BIN}"
            COMMAND "${PYTHON}" "${SEQ_TO_C}" "${SEQ_BIN}" "${SEQ_C}" "${SEQ_H}" "${SEQ_NAME}"
            DEPENDS "${SEQ_FILE}" "${SEQ_TO_C}" "${MODULE_NAME}_dictionary"
            COMMENT "Compiling sequence ${SEQ_NAME}"
            VERBATIM
        )
        list(APPEND GENERATED_SOURCES "${SEQ_C}")
    endforeach()

    set(AUTOCODER_GENERATED_BUILD_SOURCES "${GENERATED_SOURCES}" PARENT_SCOPE)
endfunction(seq_setup_autocode)
