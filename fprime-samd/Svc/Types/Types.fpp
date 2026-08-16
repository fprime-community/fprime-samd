module Svc {

    @ Scheduler Port with order argument meant to run actively on the MCU main context.
    @ Returns 'true' if any work was done (re-requests loop)
    port ActiveSched(
        context: U32 @< The call order
    ) -> bool

}
