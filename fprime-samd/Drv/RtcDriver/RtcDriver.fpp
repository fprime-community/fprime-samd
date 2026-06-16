module Samd21 {
    @ A driver for the Samd21 realtime clock
    passive component RtcDriver {

        @ Implement tick interface for rate group timing
        import Drv.Tick
        import Fw.Channel
        time get port timeGetOut

        @ Counter tracking the number of cycle overruns
        telemetry CycleOverrun: U16 update on change

    }
}
