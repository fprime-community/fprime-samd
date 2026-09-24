####
# autocoder/fpp_query.cmake:
#
# Locates `fpp-query` and wraps the configure-time half of a `static-*` autocoder:
# listing the files it is going to generate.
#
# An F Prime build autocoder is invoked twice -- once at configure time to declare its
# outputs, once at build time to write them. The declaration needs only the syntax
# model, so running a Python tool for it pays interpreter and extension-module startup
# once per module, serially, for every module in the build. `fpp-query` is a native
# executable driven by a TOML rules file, which does the same job without that cost.
####
include_guard()
include(utilities)

find_program(FPP_QUERY fpp-query)
get_filename_component(FPP_QUERY_REQUIREMENTS
    "${CMAKE_CURRENT_LIST_DIR}/../../requirements.txt" REALPATH)

if (NOT FPP_QUERY)
    message(FATAL_ERROR
        "[fpp-query] not found. Install the fprime-samd tools with:\n"
        "    pip install -r ${FPP_QUERY_REQUIREMENTS}"
    )
endif()
fprime_cmake_status("[fpp-query] found at: ${FPP_QUERY}")

# A mismatched fpp-query is worth catching here: it would declare a different set of
# paths than the generator writes, which surfaces only as a Ninja "output was never
# produced" well removed from its cause.
if (NOT FPRIME_SKIP_TOOLS_VERSION_CHECK)
    find_program(FPRIME_VERSION_CHECK NAMES fprime-version-check REQUIRED)
    execute_process(
        COMMAND "${FPRIME_VERSION_CHECK}" "fprime-fpp-query" "${FPP_QUERY_REQUIREMENTS}"
        OUTPUT_VARIABLE FPP_QUERY_EXPECTED_VERSION
        RESULT_VARIABLE FPP_QUERY_VERSION_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${FPP_QUERY}" "--version"
        OUTPUT_VARIABLE FPP_QUERY_ACTUAL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    # fprime-version-check reports `v3.3.24`; fpp-query prints `fpp-query 3.3.24`
    string(REGEX REPLACE "^v" "" FPP_QUERY_EXPECTED_VERSION "${FPP_QUERY_EXPECTED_VERSION}")
    ends_with(FPP_QUERY_VERSION_OK "${FPP_QUERY_ACTUAL_VERSION}" "${FPP_QUERY_EXPECTED_VERSION}")

    if (NOT FPP_QUERY_VERSION_RESULT EQUAL 0)
        fprime_cmake_warning(
            "[fpp-query] could not read expected version from ${FPP_QUERY_REQUIREMENTS}. Skipping check.")
    elseif (NOT FPP_QUERY_VERSION_OK)
        message(FATAL_ERROR
            "[fpp-query] version incompatible. Found '${FPP_QUERY_ACTUAL_VERSION}', "
            "expected ${FPP_QUERY_EXPECTED_VERSION}.\n"
            "    pip install -r ${FPP_QUERY_REQUIREMENTS}"
        )
    endif()
endif()

####
# Function `fpp_query_filenames`:
#
# Lists the files the autocoder described by RULES would generate from AC_INPUT_FILES,
# setting OUTPUT_VAR in the caller's scope to that (possibly empty) list of absolute
# paths. No matches is a success: a module with nothing to generate is the common case.
#
# The `fpp_info` / `fpp_autocoder_variables` pair is deliberately absent. It exists to
# produce FPP_IMPORT_FLAGS, which a syntax-only query ignores, and it requires the
# `fpp_depend` sub-build cache to already exist -- itself configure-time cost. The
# build-time half still needs both, so it calls them itself.
#
# LABEL: autocoder name, used in the error message and the cache file name
# RULES: TOML rules file naming the files the autocoder generates
# AC_INPUT_FILES: list of supported autocoder input files
# OUTPUT_VAR: name of the variable to set in the caller's scope
####
function(fpp_query_filenames LABEL RULES AC_INPUT_FILES OUTPUT_VAR)
    # Editing the rules must re-run configure, or the declared output list goes stale
    # and Ninja reports an output that was never produced
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RULES}")

    set(FILENAMES "${CMAKE_CURRENT_BINARY_DIR}/${LABEL}-filenames.txt")
    execute_process_or_fail(
        "[${LABEL}] could not list generated files"
        "${FPP_QUERY}"
        "--rules" "${RULES}"
        "-d" "${CMAKE_CURRENT_BINARY_DIR}"
        "--filenames" "${FILENAMES}"
        "--" ${AC_INPUT_FILES}
    )
    file(STRINGS "${FILENAMES}" GENERATED_FILES)
    set("${OUTPUT_VAR}" "${GENERATED_FILES}" PARENT_SCOPE)
endfunction(fpp_query_filenames)
