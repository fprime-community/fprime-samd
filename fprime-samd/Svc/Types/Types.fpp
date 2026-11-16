module Svc {

    @ Scheduler Port with order argument meant to run actively on the MCU main context.
    @ Returns 'true' if any work was done (re-requests loop)
    port ActiveSched(
        context: U32 @< The call order
    ) -> bool

}

module Samd21 {

    @ Transfer a run of parameter store bytes to or from persistent storage.
    @ Completion is reported asynchronously with a Drv.ByteStreamData carrying the same buffer.
    port PrmTransfer(
        offset: U32 @< Byte offset of the transfer within the parameter store
        ref buffer: Fw.Buffer @< The bytes to write, or the space to read into
    )

}
