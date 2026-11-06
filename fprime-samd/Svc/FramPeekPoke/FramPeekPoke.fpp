module Samd21 {
    @ Width of a peeked or poked F-RAM word
    enum FramWordWidth : U8 {
        U8_WORD = 1 @< One byte
        U16_WORD = 2 @< Two bytes, big endian
        U32_WORD = 4 @< Four bytes, big endian
    }

    @ Ground access to raw F-RAM words: read (PEEK) or write (POKE) one U8, U16 or U32 at a byte
    @ offset. For manually patching stored data (e.g. a parameter value in a Samd21.PrmStorage bank)
    @ and inspecting it (e.g. a suspected bit flip). Multi-byte words are big endian, the F Prime
    @ serialization order. A POKE bypasses every integrity check of the data it lands in.
    passive component FramPeekPoke {

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

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Read a word and report it with the Peeked event. The command completes when the read does.
        sync command PEEK(
            offset: U32 @< F-RAM byte offset of the word
            width: Samd21.FramWordWidth @< Word width
        ) opcode 0

        @ Write a word. Fails (VALIDATION_ERROR) when the value does not fit the width. The command
        @ completes when the write does; PEEK it back to confirm.
        sync command POKE(
            offset: U32 @< F-RAM byte offset of the word
            width: Samd21.FramWordWidth @< Word width
            value: U32 @< Value to write; must fit the width
        ) opcode 1

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Value read by PEEK
        event Peeked(offset: U32, width: Samd21.FramWordWidth, value: U32) \
            severity activity high \
            id 0 \
            format "F-RAM 0x{x} ({}) = 0x{x}"

        @ Value written by POKE
        event Poked(offset: U32, width: Samd21.FramWordWidth, value: U32) \
            severity activity high \
            id 1 \
            format "F-RAM 0x{x} ({}) <- 0x{x}"

        @ POKE value wider than the requested width
        event ValueTooWide(width: Samd21.FramWordWidth, value: U32) \
            severity warning low \
            id 2 \
            format "{} cannot hold value 0x{x}"

        @ The F-RAM driver rejected or failed the request
        event AccessFailed(op: Fram.Op, offset: U32, width: Samd21.FramWordWidth, $status: Fram.Status) \
            severity warning high \
            id 3 \
            format "F-RAM {} of 0x{x} ({}) failed: {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

    }
}
