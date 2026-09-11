module Samd21 {

  @ A component for managing shared memory buffers, functionally equivalent to
  @ Svc.BufferManager but tracking allocations internally with Samd21.ThinBuffer
  @ (12 bytes) instead of Fw::Buffer (44 bytes, due to its embedded
  @ Fw::ExternalSerializeBuffer) to reduce per-slot bookkeeping overhead. The
  @ Fw.BufferGet/Fw.BufferSend port boundary is unchanged, so this is a
  @ drop-in replacement for Svc.BufferManager.
  passive component ThinBufferManager {

    # ----------------------------------------------------------------------
    # General ports
    # ----------------------------------------------------------------------

    @ Mutex locked Buffer send in input port
    guarded input port bufferSendIn: Fw.BufferSend

    @ Mutex locked Buffer callee input port
    guarded input port bufferGetCallee: Fw.BufferGet

    @ Schedule input port
    guarded input port schedIn: Svc.Sched

    # ----------------------------------------------------------------------
    # Special ports
    # ----------------------------------------------------------------------

    @ Port for getting the time
    time get port timeCaller

    @ Port for emitting events
    event port eventOut

    @ Port for emitting text events
    text event port textEventOut

    @ Port for emitting Telemetry
    telemetry port tlmOut

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    include "Events.fppi"

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    include "Telemetry.fppi"

  }

}
