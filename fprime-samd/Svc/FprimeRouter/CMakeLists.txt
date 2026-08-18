####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
# UT_SOURCE_FILES: list of source files for unit tests
#
####

register_fprime_module(
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/FprimeRouter.fpp"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/FprimeRouter.cpp"
  HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/FprimeRouter.hpp"
)
