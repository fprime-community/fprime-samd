# Shared CI actions for fprime-samd consumers

## Integrate

1. Checkout with submodules, before any local-path `uses:`:

   ```yaml
   - uses: actions/checkout@v7
     with:
       submodules: recursive
   - uses: ./lib/fprime-samd/.github/actions/setup-samd-toolchain
   ```

2. Reference the actions you need by local path:

   | Action | Purpose |
   | :-- | :-- |
   | `setup-fprime` | Python, pip, F Prime requirements fan-out |
   | `setup-samd-toolchain` | arm-none-eabi-gcc + CMSIS + CMSIS-Atmel, cached |
   | `save-samd-toolchain` | Saves that cache, only after a build has succeeded |
   | `unit-tests` | Builds and runs `*_ut_exe` targets in a generated cache |
   | `measure-elf` | `size_report.py measure` against a linked ELF |
   | `render-size-report` | `size_report.py render` into comment markdown |
   | `post-size-comment` | Posts/updates the sticky comment from a `workflow_run` |

3. Make sure your repository has:
   - `lib/fprime-samd` checked out recursively
   - a `requirements.txt` that fans out to `lib/fprime/requirements.txt` and
     `lib/fprime-samd/requirements.txt`
   - `settings.ini` with `library_locations` including `./lib/fprime-samd`
   - on GitHub Enterprise Server: `checkout`, `cache`, `setup-python`,
     `upload-artifact`, `download-artifact`, and `github-script` mirrored on the
     instance, plus runner egress to github.com for the one-time `arduino-cli`
     download (or set `setup-samd-toolchain`'s `tools-archive-base` for air-gapped
     runners)

## Which deployments, which toolchain

The actions take `deployment` and `toolchain` as per-call inputs; they don't know what
deployments exist. That list lives in your own workflow.

One deployment:

```yaml
env:
  DEPLOYMENT: CuriosityReference
  TOOLCHAIN: microchip_curiosity

steps:
  - uses: ./lib/fprime-samd/.github/actions/build-deployment
    with:
      deployment: ${{ env.DEPLOYMENT }}
      toolchain: ${{ env.TOOLCHAIN }}
```

More than one: use a matrix instead, and reference `matrix.deployment` /
`matrix.toolchain` in place of the `env:` values above.

```yaml
strategy:
  matrix:
    include:
      - deployment: CuriosityReference
        toolchain: microchip_curiosity
      - deployment: OtherDeployment
        toolchain: some_other_toolchain
```
