""" fprime_samd_ci.plugins.samd_ci: SAMD21 CI plugin

Flashes a SAMD21 target through an external programmer command (e.g. OpenOCD driving the on-board nEDBG CMSIS-DAP
debugger) and waits for the target UART to enumerate. The programmer resets the target after flashing, so the flight
software is already running by the time the GDS connects to the UART; no separate launch step or console is needed.
"""
import logging
import time
from pathlib import Path

from fprime_ci.ci import Ci
from fprime_ci.plugin.definitions import plugin

LOGGER = logging.getLogger(__name__)


@plugin(Ci)
class SamdCi(Ci):
    """ SAMD21 CI plugin: flash with an external programmer, then let the GDS talk over the target UART """

    class Keys(Ci.Keys):
        """ Additional context keys used by the SAMD plugin (see Ci.Keys for the format) """
        FLASH_COMMAND = "flash-command"
        FLASH_COMMAND__ATTRS__ = (True, list)

    def __init__(self, port: str, flash_timeout: float, boot_delay: float):
        """ Set up the SAMD CI plugin """
        self.port = port
        self.flash_timeout = flash_timeout
        self.boot_delay = boot_delay

    def build(self, context: dict) -> dict:
        """ No build customization: settings.ini selects the SAMD toolchain """
        return context

    def preload(self, context: dict) -> dict:
        """ Flash the target and wait for its UART to be available """
        self.subprocess(context[self.Keys.FLASH_COMMAND], timeout=self.flash_timeout)
        self.wait_until(lambda: Path(self.port).exists(), timeout=10.0)
        if not Path(self.port).exists():
            raise RuntimeError(f"Target UART {self.port} did not appear after flashing")
        LOGGER.info("Flashed target; waiting %.1fs for boot", self.boot_delay)
        time.sleep(self.boot_delay)
        return context

    def load(self, context: dict) -> dict:
        """ Nothing to load: the image was written to flash during preload """
        return context

    def launch(self, context: dict) -> dict:
        """ Nothing to launch: the programmer reset started the flight software """
        return context

    @classmethod
    def get_name(cls):
        """ Returns the name of the plugin """
        return "samd-ci"

    @classmethod
    def get_arguments(cls):
        """ Returns the arguments of the plugin """
        return {
            ("--port",): {
                "type": str,
                "default": "/dev/samd21-curiosity",
                "help": "Target UART device shared with the GDS. Default: /dev/samd21-curiosity",
            },
            ("--flash-timeout",): {
                "type": float,
                "default": 120.0,
                "help": "Seconds allowed for the flash command to complete. Default: 120",
            },
            ("--boot-delay",): {
                "type": float,
                "default": 2.0,
                "help": "Seconds to wait after flashing for the target to boot. Default: 2",
            },
        }
