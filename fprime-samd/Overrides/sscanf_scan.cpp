// ======================================================================
// \title  format.cpp
// \author tumbar
// \brief  cpp file for c-string format function as a implementation using snprintf
// ======================================================================
#include <Fw/Types/StringUtils.hpp>
#include <Fw/Types/scan.hpp>

Fw::ScanStatus Fw::stringScan(FwSizeType& count,
                              const char* source,
                              FwSizeType maximumSize,
                              const char* formatString,
                              ...) {
    return Fw::ScanStatus::OTHER_ERROR;
}

Fw::ScanStatus Fw::stringScan(FwSizeType& count,
                              const char* source,
                              FwSizeType maximumSize,
                              const char* formatString,
                              va_list args) {
    return Fw::ScanStatus::OTHER_ERROR;
}
