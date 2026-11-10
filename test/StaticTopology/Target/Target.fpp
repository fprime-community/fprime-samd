module StaticTopology {

    enum Mode : U8 {
        OFF = 0
        ON = 1
        AUTO = 7
    }

    struct Pair {
        a: U16
        b: I16
    }

    array Triple = [3] U8

    @ Test harness: a command and telemetry endpoint. Its channels cover the types the static
    @ telemetry packetizer lays out, and its commands the responses the static command
    @ dispatcher tracks.
    passive component Target {

        @ Record a value: writes Value and increments Counter
        sync command SET_VALUE(
            value: U32
        ) opcode 0

        @ Complete with the given response
        sync command RESPOND(
            response: Fw.CmdResponse
        ) opcode 1

        @ Hold the response until the test releases it (Target::release)
        sync command HOLD opcode 2

        telemetry Value: U32 id 0
        telemetry Counter: U8 id 1
        telemetry Flag: bool id 2
        telemetry Mode: StaticTopology.Mode id 3
        telemetry Ratio: F32 id 4
        telemetry Big: I64 id 5
        telemetry Pair: StaticTopology.Pair id 6
        telemetry Triple: StaticTopology.Triple id 7

        @ In no packet
        telemetry Hidden: U16 id 8

        event ValueSet(value: U32) \
            severity activity low \
            id 0 \
            format "Value set to {}"

        time get port timeCaller

        import Fw.Command

        import Fw.Event

        import Fw.Channel

    }
}
