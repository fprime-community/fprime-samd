# Shared CI actions for fprime-samd consumers

These are the parts of a SAMD21 F Prime CI pipeline that are the same in every
repository that consumes this library: provisioning the ARM toolchain and CMSIS,
installing the Python side of F Prime, building and running the library's unit
tests, measuring the linked ELF, and posting the sticky size comment.

Everything that is *deployment*-specific -- which deployments exist, which
toolchains they build for, what the workflow triggers are, the size-comment
markers, the `EXCLUDE_FROM_ALL` list, the format-check directories -- stays in
the consuming repository's own workflow files.

## Why composite actions and not reusable workflows

A reusable workflow (`uses: owner/repo/.github/workflows/x.yml@ref`) resolves on
**the same GitHub instance as the caller**. `fprime-samd` lives on github.com,
but not every consumer does: `moonfall-power-manager` is on a GitHub Enterprise
Server instance, and a `workflow_call` from there cannot reach github.com. A
composite action referenced by local path, on the other hand, is just a
directory in the workspace:

```yaml
- uses: actions/checkout@v7
  with:
    submodules: recursive          # must precede any local-path `uses:`
- uses: ./lib/fprime-samd/.github/actions/setup-samd-toolchain
```

That works identically on github.com and on Enterprise Server, and it pins the
actions to the `fprime-samd` submodule commit the consumer already vendors --
so a CI change arrives with the submodule bump that carries it, reviewably,
instead of floating on someone else's default branch.

The trade-off is that a composite action cannot declare job-level `permissions`,
`concurrency`, a trigger, or a matrix. Those stay in the caller, which is where
they belong anyway.

## The actions

| Action | Purpose |
| :-- | :-- |
| `setup-fprime` | Python, pip, and the F Prime requirements fan-out |
| `setup-samd-toolchain` | arm-none-eabi-gcc + CMSIS + CMSIS-Atmel, cached |
| `save-samd-toolchain` | Saves that cache, but only after a build has succeeded |
| `unit-tests` | Builds and runs `*_ut_exe` targets in a generated cache |
| `measure-elf` | `size_report.py measure` against a linked ELF |
| `render-size-report` | `size_report.py render` into comment markdown |
| `post-size-comment` | Posts/updates the sticky comment from a `workflow_run` |

`setup-samd-toolchain` and `save-samd-toolchain` are deliberately two actions
rather than one. The cache is saved only *after* the real build has succeeded
against the (pruned) tree, so a bad prune can never be cached; a single action
cannot straddle the build step.

## Prerequisites in the consuming repository

* `lib/fprime-samd` checked out (`submodules: recursive`) **before** the first
  local-path `uses:`.
* A `requirements.txt` that fans out to `lib/fprime/requirements.txt` and
  `lib/fprime-samd/requirements.txt`.
* `settings.ini` with `library_locations` including `./lib/fprime-samd`.
* On GitHub Enterprise Server: the `actions/*` actions these wrap
  (`checkout`, `cache`, `setup-python`, `upload-artifact`, `download-artifact`,
  `github-script`) must be mirrored on the instance, and the runner needs egress
  to github.com for the one-time `arduino-cli` toolchain download. See
  `setup-samd-toolchain`'s `tools-archive-base` input for the air-gapped escape
  hatch.
