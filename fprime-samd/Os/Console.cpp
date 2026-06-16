// ======================================================================
// \title Os/Stub/Console.cpp
// \brief stub implementation for Os::Console
// ======================================================================
#include <fprime-samd/Os/Console.hpp>

namespace Os {
namespace Samd21 {

void StreamConsoleHandle::setStreamHandler() {
}

void StreamConsole::writeMessage(const CHAR *message, const FwSizeType size) {
    (void) message;
}

ConsoleHandle* StreamConsole::getHandle() {
    return &this->m_handle;
}


} // namespace Samd21
} // namespace Os
