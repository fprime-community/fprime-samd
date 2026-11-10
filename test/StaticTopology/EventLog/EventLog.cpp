// ======================================================================
// \title  EventLog.cpp
// \brief  Test harness event recorder of the StaticTopology
// ======================================================================

#include "test/StaticTopology/EventLog/EventLog.hpp"
#include "Fw/Types/Assert.hpp"

namespace StaticTopology {

constexpr FwSizeType EventLog::HISTORY;

EventLog::EventLog(const char* const compName) : EventLogComponentBase(compName), m_events(), m_count(0) {}

EventLog::~EventLog() {}

void EventLog::reset() {
    this->m_count = 0;
}

FwSizeType EventLog::getCount() const {
    return this->m_count;
}

FwSizeType EventLog::count(FwEventIdType id) const {
    FwSizeType found = 0;
    const FwSizeType kept = (this->m_count < HISTORY) ? this->m_count : HISTORY;
    for (FwSizeType i = 0; i < kept; i++) {
        found += (this->m_events[i].id == id) ? 1 : 0;
    }
    return found;
}

const EventLog::Event* EventLog::last(FwEventIdType id) const {
    const FwSizeType kept = (this->m_count < HISTORY) ? this->m_count : HISTORY;
    for (FwSizeType i = kept; i > 0; i--) {
        if (this->m_events[i - 1].id == id) {
            return &this->m_events[i - 1];
        }
    }
    return nullptr;
}

const EventLog::Event& EventLog::get(FwSizeType index) const {
    FW_ASSERT(index < this->m_count && index < HISTORY, static_cast<FwAssertArgType>(index));
    return this->m_events[index];
}

void EventLog::logIn_handler(FwIndexType portNum,
                             FwEventIdType id,
                             Fw::Time& timeTag,
                             const Fw::LogSeverity& severity,
                             Fw::LogBuffer& args) {
    if (this->m_count < HISTORY) {
        this->m_events[this->m_count].id = id;
        this->m_events[this->m_count].severity = severity;
        this->m_events[this->m_count].args = args;
    }
    this->m_count++;
}

}  // namespace StaticTopology
