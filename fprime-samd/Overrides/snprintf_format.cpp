// ======================================================================
// \title  format.cpp
// \author tumbar
// \brief  cpp file for c-string format function as a implementation using snprintf
// ======================================================================
#include <Fw/Types/format.hpp>

Fw::FormatStatus Fw::stringFormat(char* destination, const FwSizeType maximumSize, const char* formatString, ...) {
    return Fw::FormatStatus::OTHER_ERROR;
}

Fw::FormatStatus Fw::stringFormat(char* destination,
                                  const FwSizeType maximumSize,
                                  const char* formatString,
                                  va_list args) {
    return Fw::FormatStatus::OTHER_ERROR;
}
