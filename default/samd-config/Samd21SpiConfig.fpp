# Samd21.SpiPorts was retired with the Samd21-private asynchronous SPI interface: the
# SpiDriver's slot count is now Drv.AsyncSpiCfg.SpiDriverPorts (config/AsyncSpiCfg.fpp,
# F Prime default 10; override it in the project's configuration module to match the
# number of chip selects actually wired, since every slot costs two pending-reply
# records -- outcome and refusal -- plus three port objects (request, completion,
# chip select) in builds without FW_DIRECT_PORT_CALLS).
module Samd21 {
}
