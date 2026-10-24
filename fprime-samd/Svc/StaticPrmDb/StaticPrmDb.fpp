module Samd21 {
    @ A small bare-metal implementation of PrmDb
    passive component StaticPrmDb {

        ##############################################################################
        #### PrmDb ports (framework interface) ####
        ##############################################################################

        @ Port to get parameter values
        guarded input port getPrm: Fw.PrmGet

        @ Port to update parameters
        sync input port setPrm: Fw.PrmSet

        ##############################################################################
        #### IO Ports ####
        ##############################################################################

        enum PrmBank : U8 {
            PRIME
            BACKUP
        }

        enum IoState : U8 {
            IDLE     @< The transmit buffer is not 'in-flight'
            READING  @< The transmit buffer is currently reading from the non-volatile
            WRITING  @< The transmit buffer is currently writing to the non-volatile
        }

        @ Send the parameter value buffer out for writing
        output port writeOut: [2] Fw.BufferSend

        @ Asynchronous reply for write completion.
        sync input port writeReturnIn: [2] Drv.ByteStreamData

        @ Send the parameter value buffer out for reading
        output port readOut: [2] Fw.BufferSend

        @ Asynchronous reply for read completion.
        sync input port readReturnIn: [2] Drv.ByteStreamData

        ##############################################################################
        #### Commands ####
        ##############################################################################

        @ Save the current parameters to non-volatile memory
        sync command SAVE(
            @ The non-volatile parameter bank to write to
            bank: PrmBank
        ) \
            opcode 0

        @ Load parameter values from non-volatile memory
        sync command LOAD(
            @ The non-volatile parameter bank to read from
            bank: PrmBank
        ) \
            opcode 1

        @ The deployment parameter database checksum computed statically by the build
        telemetry DatabaseChecksum: U32 id 0

        event DroppingPrmSave($id: FwPrmIdType, $state: IoState) \
            severity warning high \
            id 0 \
            format "Dropping parameter DB save id=0x{x} during {} transaction"

        event MismatchedParameterDatabaseHash(
            bank: PrmBank
            stored: U32
            inMemory: U32
        ) \
            severity warning high \
            id 1 \
            format "Mismatched parameter database hash in bank {}, stored=0x{x}, in-memory=0x{x}"

        event LoadedDatabase(
            bank: PrmBank
            validRecords: U32
            invalidRecords: U32
        ) \
            severity activity high \
            id 2 \
            format "Successfully loaded parameter database from bank {} with {} valid records and {} invalid records"

        event ReadFailed(bank: PrmBank, status: Drv.ByteStreamStatus) \
            severity warning high \
            id 3 \
            format "Failed to read from bank {}: {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

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
