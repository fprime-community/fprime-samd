module StaticTopology {

    @ Test harness: records the events of every instance in the StaticTopology
    passive component EventLog {

        sync input port logIn: Fw.Log

    }
}
