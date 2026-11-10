module StaticTopology {

    @ Topology the Static* components' unit tests run against. Each test-harness instance
    @ stands in for what surrounds the static components in a flight deployment.
    deployment topology Top {

        instance cmdDisp
        instance tlm
        instance target
        instance target2
        instance ground
        instance sequencer
        instance eventLog
        instance clock

        @ Packet ids are not contiguous and do not start at 0. The packetizer's pktSendIn port
        @ index is the packet id (NUM_TLM_PACKETS = 1 by default: only id 0 is port driven).
        telemetry packets Packets {

            packet Primary id 0 group 0 {
                target.Value
                target.Counter
                target2.Value
            }

            @ Every channel type, and channels already in Primary (a channel may be in
            @ several packets)
            packet Types id 5 group 1 {
                target.Flag
                target.Mode
                target.Ratio
                target.Big
                target.Pair
                target.Triple
                target.Value
            }

            packet Empty id 9 group 1 {
            }

        } omit {
            target.Hidden
            target2.Counter
            target2.Flag
            target2.Mode
            target2.Ratio
            target2.Big
            target2.Pair
            target2.Triple
            target2.Hidden
        }

        command connections instance cmdDisp

        event connections instance eventLog

        telemetry connections instance tlm

        time connections instance clock

        connections Ground {
            ground.cmdBuffOut -> cmdDisp.seqCmdBuff[0]
            cmdDisp.seqCmdStatus[0] -> ground.cmdStatusIn
            sequencer.cmdBuffOut -> cmdDisp.seqCmdBuff[1]
            sequencer.cmdOut -> cmdDisp.seqCmdIn[1]
            cmdDisp.seqCmdStatus[1] -> sequencer.cmdStatusIn

            ground.pktSendOut[0] -> tlm.pktSendIn[0]
            tlm.pktSendOut[0] -> ground.pktRecvIn[0]
        }

    }
}
