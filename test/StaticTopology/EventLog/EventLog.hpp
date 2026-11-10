// ======================================================================
// \title  EventLog.hpp
// \brief  Test harness event recorder of the StaticTopology
// ======================================================================

#ifndef StaticTopology_EventLog_HPP
#define StaticTopology_EventLog_HPP

#include "test/StaticTopology/EventLog/EventLogComponentAc.hpp"

namespace StaticTopology {

class EventLog final : public EventLogComponentBase {
  public:
    //! Events kept; further events are counted but not kept
    static constexpr FwSizeType HISTORY = 32;

    struct Event {
        FwEventIdType id;
        Fw::LogSeverity severity;
        Fw::LogBuffer args;
    };

    explicit EventLog(const char* const compName);
    ~EventLog();

    void reset();

    //! Events received since the last reset
    FwSizeType getCount() const;

    //! Events with this ID received since the last reset
    FwSizeType count(FwEventIdType id) const;

    //! The last event with this ID, nullptr when there is none
    const Event* last(FwEventIdType id) const;

    const Event& get(FwSizeType index) const;

  private:
    void logIn_handler(FwIndexType portNum,
                       FwEventIdType id,
                       Fw::Time& timeTag,
                       const Fw::LogSeverity& severity,
                       Fw::LogBuffer& args) override;

    Event m_events[HISTORY];
    FwSizeType m_count;
};

}  // namespace StaticTopology

#endif
