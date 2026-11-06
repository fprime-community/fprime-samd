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

[`sample-configs/samd21g17.yml`](sample-configs/samd21g17.yml) is the `fprime-ci` configuration for the
`CuriosityReference` deployment of `fprime-samd-reference`; copy and adapt it for other deployments. It boils down to:

```yaml
deployment-name: CuriosityReference
platform-name: samd21g17
dictionary: build-artifacts/samd21g17/CuriosityReference/dict/TopTopologyDictionary.json
executable: build-artifacts/samd21g17/CuriosityReference/bin/CuriosityReference.elf.bin
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
  - cmsis_dap_vid_pid 0x03eb 0x2175   # the Nano's nEDBG, not whichever CMSIS-DAP probe enumerates first
  - -c
  - transport select swd
  - -f
  - target/at91samdXX.cfg
  - -c
  - program build-artifacts/samd21g17/CuriosityReference/bin/CuriosityReference.elf.bin 0x00000000 verify reset exit
```

## GitHub Actions

Split the flow across a build host and the hardware runner, as `nasa/fprime`'s `ext-fprime-zephyr-reference.yml`
does: the build job runs `fprime-ci -c <config> --add-stage build` and uploads `archive.tar.gz`, the hardware job
downloads it and runs `fprime-ci -c <config> --skip-stage build`. Point the consuming repository's
`.github/actions/setup` at [`.github/actions/setup-ci`](../.github/actions/setup-ci/action.yml), which installs this
plugin and (for the `build` stage only) the SAMD21 cross toolchain.

## Hardware runner

The self-hosted runner attached to the board needs `openocd` (0.12 or newer) and read/write access to the nEDBG
CMSIS-DAP HID interface and its CDC UART. Install `udev/91-samd21-curiosity.rules` into `/etc/udev/rules.d/` to get
a stable `/dev/samd21-curiosity` UART symlink regardless of USB enumeration order, and add the runner user to the
`dialout` and `plugdev` groups.
