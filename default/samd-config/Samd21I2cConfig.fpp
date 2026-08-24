module Samd21 {
    @ Number of client ports on each I2cDriver (i.e. how many components may
    @ share a single I2C bus driver). Sizes the write/read/writeRead request
    @ ports and their completion callbacks.
    constant I2cClientPorts = 2
}
