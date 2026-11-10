module StaticTopology {

    @ Test harness: a time source the test sets, so time tags in packets are deterministic
    passive component Clock {

        sync input port timeGetPort: Fw.Time

    }
}
