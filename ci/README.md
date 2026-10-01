# fprime-samd-ci

[fprime-ci](https://github.com/fprime-community/fprime-ci) plugin for running F Prime integration tests against
SAMD21 hardware. The plugin adds a `samd-ci` selection that:

1. runs the configured `flash-command` (OpenOCD over the on-board nEDBG CMSIS-DAP debugger, by default),
2. waits for the target UART (`--port`) to enumerate and the target to boot (`--boot-delay`),
3. leaves the rest to the standard `fprime-ci` flow: the GDS attaches to the same UART and `pytest` runs the
   configured integration tests.

## Install

```bash
pip install ./lib/fprime-samd/ci
```

## Configure

`fprime-ci` configuration for a Curiosity Nano deployment (see `fprime-samd-reference/ci/curiosity-nano.yml`):

```yaml
deployment-name: CuriosityReference
platform-name: microchip_curiosity
dictionary: build-artifacts/microchip_curiosity/CuriosityReference/dict/TopTopologyDictionary.json
executable: build-artifacts/microchip_curiosity/CuriosityReference/bin/CuriosityReference.elf.bin
test-scripts:
  - CuriosityReference/test/int/test_curiosity_reference.py
archive-path: ./archive.tar.gz
extra-gds-arguments: [--communication-selection, uart, --uart-device, /dev/samd21-curiosity, --uart-baud, "115200",
                      --framing-selection, fprime, --packet-set-name, Main, --gui, none]
command-line-options:
  ci-selection: samd-ci
  port: /dev/samd21-curiosity
flash-command:
  - openocd
  - -f
  - interface/cmsis-dap.cfg
  - -c
  - transport select swd
  - -f
  - target/at91samdXX.cfg
  - -c
  - program build-artifacts/microchip_curiosity/CuriosityReference/bin/CuriosityReference.elf.bin 0x00000000 verify reset exit
```

## Hardware runner

The self-hosted runner attached to the board needs `openocd` (0.12 or newer) and read/write access to the nEDBG
CMSIS-DAP HID interface and its CDC UART. Install `udev/91-samd21-curiosity.rules` into `/etc/udev/rules.d/` to get
a stable `/dev/samd21-curiosity` UART symlink regardless of USB enumeration order, and add the runner user to the
`dialout` and `plugdev` groups.
