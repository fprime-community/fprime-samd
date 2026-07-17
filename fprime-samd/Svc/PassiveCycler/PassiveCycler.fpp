module Svc {
    @ A passive component for running schedOut and exposes a public function for cycling a topology
    passive component PassiveCycler {

        output port cycleOut: [PassiveRateGroupOutputPorts] Svc.Sched

    }
}