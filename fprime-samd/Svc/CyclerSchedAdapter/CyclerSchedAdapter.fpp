module Samd21 {
    @ Drives one Svc.Sched consumer from the main-loop cycler (Svc.PassiveCycler).
    @ Each activeIn call is forwarded to schedOut with the context unchanged and the
    @ adapter always reports that no further work is pending, so the consumer needs
    @ only a plain schedIn port and is not aware of the cycler protocol.
    passive component CyclerSchedAdapter {

        @ Turn from Svc.PassiveCycler.cycleOut; always returns false (no work pending)
        sync input port activeIn: Svc.ActiveSched

        @ The forwarded turn; context is the activeIn context unchanged — under
        @ Svc.PassiveCycler that is the cycleOut slot index, not a rate-group
        @ context; consumers shall not interpret it
        output port schedOut: Svc.Sched

    }
}
