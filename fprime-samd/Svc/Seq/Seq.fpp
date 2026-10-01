module Samd21 {
    @ A component for dispatching a static command list on a schedule
    passive component Seq {

        @ Active in for dispatching the "next" action in the table or reseting
        sync input port activeIn: Svc.ActiveSched

        @ Check timers for wait/wait_until timeouts
        sync input port schedIn: Svc.Sched

        @ Run an action table after the current execution finishes
        @ When this component enters idle, it will pick up this pending run
        sync command RUN(table: Samd21.SeqNames) opcode 0

        @ Set the table to run if we exit with an error (i.e. command failure).
        @ The pending `RUN` is cleared.
        sync command RUN_ON_ERROR(table: Samd21.SeqNames) opcode 1

        @ Stop any executing or pending action and reset the program counter to the
        @ start of the sequence without dispatching anything. A command response for
        @ an already-dispatched action may still arrive afterwards; it is dropped.
        sync command CANCEL opcode 2

        @ Block for a number of rate groups ticks before continuing
        @ If n == 0, the sequence will resume on the NEXT rate group tick
        sync command WAIT_TICKS(n: U32) opcode 3

        @ Block the sequence until the given timeBase matches and the time >= seconds.useconds
        sync command WAIT_UNTIL(
            timeBase: TimeBase  @< Wait for the FSW to reach this timebase
            seconds: U32        @< Wait for FSW time to reach this time in seconds
            useconds: U32       @< Microsecond offset to wait for FSW
        ) \
            opcode 4

        @ Port to dispatch commands (each action). Wired to a command dispatcher's
        @ Fw.Cmd sequencer input (e.g. Baremetal.PassiveCmdDispatcher.seqCmdIn).
        @ Commands are dispatched as raw (opcode, args) with no packet descriptor.
        output port commandOut: Fw.Com

        @ Port to receive status of the currently executing action
        sync input port commandResponseIn: Fw.CmdResponse

        event StartingSequence(name: SeqNames) \
            severity activity low \
            format "Starting sequence {}"

        event InvalidSequence(len: U32, offset: U16) \
            severity warning high \
            format "Invalid command length {} at offset 0x{x}"

        event DroppingStrayResponse(
            opCode: FwOpcodeType
            offset: U16
            response: Fw.CmdResponse
        ) \
            severity warning high \
            format "Dropping stray command response with opcode 0x{x} at offset 0x{x} response {}"

        event CommandFailed(
            opCode: FwOpcodeType
            offset: U16
            response: Fw.CmdResponse
        ) \
            severity warning low \
            format "Command opcode 0x{x} at offset 0x{x} failed with {}"

        event Cancelled(tableIdx: U8, offset: U16) \
            severity activity high \
            format "Cancelled action {} at offset 0x{x}"

        event FinishedSequence(name: SeqNames) \
            severity activity low \
            format "Finished sequence {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        import Fw.Event

        # @ Enables telemetry channels handling
        # import Fw.Channel

    }
}
