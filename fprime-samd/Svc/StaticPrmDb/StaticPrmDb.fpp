module Samd21 {
    @ static-prm-db
    @ A small bare-metal implementation of PrmDb
    passive component StaticPrmDb {

        ##############################################################################
        #### PrmDb ports (framework interface) ####
        ##############################################################################

        @ Port to get parameter values
        sync input port getPrm: Fw.PrmGet

        @ Port to update parameters
        sync input port setPrm: Fw.PrmSet

        ##############################################################################
        #### IO Ports ####
        ##############################################################################

        enum IoState : U8 {
            IDLE     @< No save or load is in progress
            READING  @< A load is reading records from the persistent store
            WRITING  @< A save is writing records to the persistent store
        }

        @ How a load ended
        enum LoadResult : U8 {
            COMPLETE     @< Every slot of the store was read
            READ_FAILED  @< Storage failed a read; only the slots before it were read
        }

        @ How a load combines the persistent store with the values already in memory
        enum LoadMode : U8 {
            REPLACE  @< Take each parameter whole from the store; a parameter the store does not hold becomes invalid
            MERGE    @< Keep the values already valid in memory; take only the rest, whole, from the store
        }

        @ Write a run of records to the persistent store
        output port writeOut: Samd21.PrmTransfer

        @ Asynchronous reply for write completion.
        sync input port writeReturnIn: Drv.ByteStreamData

        @ Read a run of records from the persistent store
        output port readOut: Samd21.PrmTransfer

        @ Asynchronous reply for read completion.
        sync input port readReturnIn: Drv.ByteStreamData

        @ Main-context scheduler: acts on the last transfer's completion and issues the next
        @ transfer, at most one per call. Returns true when it did work.
        sync input port activeIn: Svc.ActiveSched

        ##############################################################################
        #### Commands ####
        ##############################################################################

        @ Save the current parameters to non-volatile memory
        sync command SAVE opcode 0

        @ Load parameter values from non-volatile memory
        sync command LOAD(
            mode: LoadMode @< REPLACE for the first store loaded, MERGE to fill in from a redundant store
        ) opcode 1

        @ Parameter value bytes per persistent record. Also brings the configuration into every
        @ model using this component, where static-prm-db reads it.
        constant RECORD_VALUE_SIZE = Samd21.StaticPrmDbConfig.RECORD_VALUE_SIZE

        @ Records of the last load that passed their CRC and belong to a parameter
        telemetry ValidRecords: U32 id 0 format "{} records"

        @ Records of the last load that failed their CRC
        telemetry CorruptRecords: U32 id 1 format "{} records"

        @ Parameters with no valid value after the last load
        telemetry InvalidParameters: U32 id 2 format "{} parameters"

        @ Parameters of the last load whose records all loaded but were of different saves
        telemetry IncoherentParameters: U32 id 3 format "{} parameters"

        event DroppingPrmSave($id: FwPrmIdType, $state: IoState) \
            severity warning high \
            id 0 \
            format "Dropping parameter DB save id=0x{x} during {} transaction"

        event LoadedDatabase(
            mode: LoadMode
            result: LoadResult
            validRecords: U32
            corruptRecords: U32
            unknownRecords: U32
            incoherentParameters: U32
            invalidParameters: U32
        ) \
            severity activity high \
            id 1 \
            format "Loaded parameter store ({}, {}): {} valid, {} corrupt and {} unknown records, {} parameters incoherent, {} invalid"

        event ReadFailed(status: Drv.ByteStreamStatus) \
            severity warning high \
            id 2 \
            format "Failed to read parameters: {}"

        event NoStorage($state: IoState) \
            severity warning high \
            id 4 \
            format "Cannot start {} transfer: no persistent storage is connected"

        @ A successful save emits no event (saves may run every second); a failed one is reported
        @ here, throttled, and the throttle is cleared by the next successful save
        event WriteFailed(status: Drv.ByteStreamStatus) \
            severity warning high \
            id 3 \
            format "Failed to write parameters: {}" \
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

        @ Port to return the value of a parameter
        param get port prmGet

        @Port to set the value of a parameter
        param set port prmSet

    }
}
