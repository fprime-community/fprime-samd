# The SAMD21-private asynchronous SPI ports and interface (Samd21.SpiWriteRead, Samd21.SpiReply,
# Samd21.AsyncSpi) were retired when F Prime gained Drv.AsyncSpi / Drv.AsyncGuardedSpi
# (nasa/fprime #6237). Samd21.SpiDriver implements Drv.AsyncGuardedSpi; clients use
# Drv.SpiWriteReadRequest / Drv.SpiWriteReadCallback and size their port arrays with
# Drv.AsyncSpiCfg.SpiDriverPorts.
module Samd21 {
}
