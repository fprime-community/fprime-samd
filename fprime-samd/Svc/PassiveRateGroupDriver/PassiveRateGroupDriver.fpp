module Samd21 {
    @ A rate group driver component with input and output cycle ports meant for driving the blocking PassiveRateGroup components
    passive component PassiveRateGroupDriver {

        @ Cycle input to the rate group driver
        sync input port CycleIn: Svc.Cycle

        @ Cycle output from the rate group driver
        output port CycleOut: [RateGroupDriverRateGroupPorts] Svc.Cycle

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables telemetry channels handling
        import Fw.Channel

        @ Max execution time of rate group cycle
        telemetry MaxCycleTime: U32 update on change format "{} us"

        @ Execution time of current cycle
        telemetry CycleTime: U32 format "{} us"

    }
}