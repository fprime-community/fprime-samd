module StaticTopology {

    @ Test harness: stands in for the ground or a sequencer. Sends commands into the
    @ dispatcher, records the responses it sends back, triggers packets and records the
    @ packets the packetizer sends. One instance per dispatcher sequence port: FPP matches
    @ seqCmdStatus[i] with seqCmdBuff[i], so an instance holds only one of them.
    passive component Ground {

        @ Command packets, as a router or sequencer sends them (dispatcher seqCmdBuff)
        output port cmdBuffOut: Fw.Com

        @ Commands, as a sequencer sends them (dispatcher seqCmdIn)
        output port cmdOut: Fw.Cmd

        @ Command responses (dispatcher seqCmdStatus)
        sync input port cmdStatusIn: Fw.CmdResponse

        @ Rate group style packet triggers (packetizer pktSendIn)
        output port pktSendOut: [Samd21.StaticTlmPacketizerConfig.NUM_TLM_PACKETS] Svc.Sched

        @ Packets from the packetizer (packetizer pktSendOut)
        sync input port pktRecvIn: [Samd21.StaticTlmPacketizerConfig.NUM_PKT_OUT] Fw.Com

    }
}
