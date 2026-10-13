module Samd21 {
    @ Reports processor time (TB_PROC_TIME) from the Samd21 realtime clock
    passive component ProcTime {

        @ Port to retrieve time
        sync input port getTime: Fw.Time

    }
}
