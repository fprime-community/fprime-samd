module Samd21 {
    @ Measure time on the Samd21 through Arduino time
    passive component Samd21Time {

        @ Port to retrieve time
        sync input port getTime: Fw.Time

    }
}
