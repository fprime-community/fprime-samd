module StaticTopology {

    @ Base id of instances with no dictionary items
    constant NO_DICTIONARY = 0xFFFF

    instance cmdDisp: Samd21.StaticCmdDispatcher base id 0x0100

    instance tlm: Samd21.StaticTlmPacketizer base id 0x0200

    @ Two instances of one component: same local opcodes and channel ids at different bases
    instance target: StaticTopology.Target base id 0x1000
    instance target2: StaticTopology.Target base id 0x2000

    @ Ground on dispatcher sequence port 0 (Fw.Com packets) and the packetizer
    instance ground: StaticTopology.Ground base id NO_DICTIONARY

    @ Sequencer on dispatcher sequence port 1 (Fw.Com packets and Fw.Cmd)
    instance sequencer: StaticTopology.Ground base id NO_DICTIONARY
    instance eventLog: StaticTopology.EventLog base id NO_DICTIONARY
    instance clock: StaticTopology.Clock base id NO_DICTIONARY

}
