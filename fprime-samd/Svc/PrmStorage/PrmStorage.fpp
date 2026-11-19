module Samd21 {
    @ Adapts the Samd21.StaticPrmDb store transfers to offset-addressed, size-limited Fram
    @ requests. Each transfer is moved as-is, in Fram.FramConfig.MaxTransfer chunks, at its
    @ offset within the selected bank; validating the records is the database's job. The
    @ PrmStorageConfig::NUM_BANKS banks are packed back to back from PrmStorageConfig::BASE_OFFSET,
    @ each holding one store of StaticPrmDbConfig::STORE_RECORDS records.
    passive component PrmStorage {

        # ----------------------------------------------------------------------
        # Parameter database interface (Samd21.StaticPrmDb writeOut / readOut)
        # ----------------------------------------------------------------------

        @ Write a run of store bytes to the selected bank
        sync input port writeIn: Samd21.PrmTransfer

        @ Completion of writeIn: OP_OK when every byte was written, OTHER_ERROR otherwise
        output port writeReturnOut: Drv.ByteStreamData

        @ Read a run of store bytes from the selected bank
        sync input port readIn: Samd21.PrmTransfer

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

        @ Point the following saves and loads at a bank. BUSY while a transfer is in progress,
        @ VALIDATION_ERROR when the bank does not exist.
        sync command SELECT_BANK(
            bank: U8 @< Bank index, 0 to PrmStorageConfig::NUM_BANKS - 1
        ) opcode 0

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Bank the next save or load uses
        telemetry ActiveBank: U8 id 0

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ An F-RAM request failed; the transfer was ended with OTHER_ERROR
        event StorageFailed(op: Fram.Op, bank: U8, offset: U32, $status: Fram.Status) \
            severity warning high \
            id 0 \
            format "F-RAM {} of parameter bank {} at offset {} failed: {}" \
            throttle 5

        @ SELECT_BANK named a bank that does not exist; the selection is unchanged
        event InvalidBank(bank: U8, numBanks: U8) \
            severity warning low \
            id 1 \
            format "Parameter bank {} does not exist ({} banks)"

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
