""" Tests for the fprime_samd_ci.plugins.samd_ci preload (flash, wait for UART, boot delay) """
import importlib
import sys
import types
from unittest import mock

import pytest


@pytest.fixture
def samd_ci():
    """ Import samd_ci with stand-ins for fprime-ci and fprime-gds, which are not installable from PyPI here """
    class Ci:
        class Keys:
            pass

    modules = {name: types.ModuleType(name) for name in [
        "fprime_gds", "fprime_gds.plugin", "fprime_gds.plugin.definitions",
        "fprime_ci", "fprime_ci.ci", "fprime_ci.plugin", "fprime_ci.plugin.definitions",
    ]}
    modules["fprime_ci.ci"].Ci = Ci
    modules["fprime_ci.plugin.definitions"].plugin = lambda _: (lambda cls: cls)
    with mock.patch.dict(sys.modules, modules):
        sys.modules.pop("fprime_samd_ci.plugins.samd_ci", None)
        yield importlib.import_module("fprime_samd_ci.plugins.samd_ci")
    sys.modules.pop("fprime_samd_ci.plugins.samd_ci", None)


def make_plugin(samd_ci, port, flash_timeout=12.5, boot_delay=0.0):
    plugin = samd_ci.SamdCi(str(port), flash_timeout, boot_delay)
    plugin.subprocess = mock.Mock(return_value=(None, None, (None, None)))
    plugin.wait_until = mock.Mock()
    return plugin


def test_preload_flashes_then_waits_for_port(samd_ci, tmp_path):
    port = tmp_path / "ttyACM0"
    port.touch()
    plugin = make_plugin(samd_ci, port, boot_delay=0.25)
    with mock.patch.object(samd_ci.time, "sleep") as sleep_mock:
        context = plugin.preload({"flash-command": ["openocd", "-c", "program image.bin verify reset exit"]})
    plugin.subprocess.assert_called_once_with(["openocd", "-c", "program image.bin verify reset exit"], timeout=12.5)
    plugin.wait_until.assert_called_once()
    sleep_mock.assert_called_once_with(0.25)
    assert context == {"flash-command": ["openocd", "-c", "program image.bin verify reset exit"]}


def test_preload_fails_when_port_never_appears(samd_ci, tmp_path):
    plugin = make_plugin(samd_ci, tmp_path / "ttyACM0")
    with pytest.raises(RuntimeError, match="did not appear"):
        plugin.preload({"flash-command": ["true"]})
    plugin.subprocess.assert_called_once_with(["true"], timeout=12.5)


def test_preload_propagates_flash_failure_without_waiting(samd_ci, tmp_path):
    port = tmp_path / "ttyACM0"
    port.touch()
    plugin = make_plugin(samd_ci, port)
    plugin.subprocess.side_effect = RuntimeError("openocd exited 1")
    with pytest.raises(RuntimeError, match="openocd exited 1"):
        plugin.preload({"flash-command": ["openocd"]})
    plugin.wait_until.assert_not_called()


def test_build_load_launch_are_no_ops(samd_ci, tmp_path):
    plugin = make_plugin(samd_ci, tmp_path / "ttyACM0")
    context = {"flash-command": ["true"]}
    assert plugin.build(context) is context
    assert plugin.load(context) is context
    assert plugin.launch(context) is context
    plugin.subprocess.assert_not_called()


def test_name_and_flash_command_key(samd_ci):
    assert samd_ci.SamdCi.get_name() == "samd-ci"
    assert samd_ci.SamdCi.Keys.FLASH_COMMAND == "flash-command"
    assert samd_ci.SamdCi.Keys.FLASH_COMMAND__ATTRS__ == (True, list)


def test_argument_defaults_match_udev_rule(samd_ci):
    arguments = samd_ci.SamdCi.get_arguments()
    assert arguments[("--port",)]["default"] == "/dev/samd21-curiosity"
    assert arguments[("--flash-timeout",)]["default"] == 120.0
    assert arguments[("--boot-delay",)]["default"] == 2.0
