module Samd21 {
    @ Region of persistent storage the parameter database is saved to and loaded from
    enum PrmBank : U8 {
        PRIME = 0 @< Selected at boot
        BACKUP = 1 @< Redundant copy
    }

    @ Adapts the Samd21.StaticPrmDb whole-image buffer interface to offset-addressed,
    @ size-limited Fram requests. The image is transferred as-is, in Fram.FramConfig.MaxTransfer
    @ chunks, starting at the offset of the selected bank; validating it is the database's job.
    passive component PrmStorage {

        # ----------------------------------------------------------------------
        # Parameter database interface (Samd21.StaticPrmDb writeOut / readOut)
        # ----------------------------------------------------------------------

        @ Write the database image to the selected bank
        sync input port writeIn: Fw.BufferSend

        @ Completion of writeIn: OP_OK when every byte was written, OTHER_ERROR otherwise
        output port writeReturnOut: Drv.ByteStreamData

        @ Read the selected bank into the database image
        sync input port readIn: Fw.BufferSend

        @ Completion of readIn: OP_OK when every byte was read, OTHER_ERROR otherwise
        output port readReturnOut: Drv.ByteStreamData

        # ----------------------------------------------------------------------
        # F-RAM interface (one Fram.FramDriver client index)
        # ----------------------------------------------------------------------

        @ Read request to the F-RAM driver
        output port framRead: Fram.Request

        @ Completion of an accepted framRead
        sync input port framReadComplete: Fram.Complete

        @ Write request to the F-RAM driver
        output port framWrite: Fram.Request

        @ Completion of an accepted framWrite
        sync input port framWriteComplete: Fram.Complete

        @ Retries an F-RAM request that was rejected BUSY (e.g. during the driver's startup reads)
        sync input port schedIn: Svc.Sched

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Point the following saves and loads at a bank. BUSY while a transfer is in progress.
        sync command SELECT_BANK(bank: Samd21.PrmBank) opcode 0

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Bank the next save or load uses
        telemetry ActiveBank: Samd21.PrmBank id 0

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ The following saves and loads use this bank
        event BankSelected(bank: Samd21.PrmBank) \
            severity activity high \
            id 0 \
            format "Parameter storage bank set to {}"

        @ An F-RAM request failed; the transfer was ended with OTHER_ERROR
        event StorageFailed(op: Fram.Op, bank: Samd21.PrmBank, offset: U32, $status: Fram.Status) \
            severity warning high \
            id 1 \
            format "F-RAM {} of parameter {} bank at offset {} failed: {}" \
            throttle 5

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}
