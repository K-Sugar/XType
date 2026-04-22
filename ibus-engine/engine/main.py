#!/usr/bin/env python3
"""XType IBus engine entry point."""

import logging
import os
import sys

import gi
gi.require_version("IBus", "1.0")
from gi.repository import IBus

from .engine import CotypistEngine

logging.basicConfig(
    level=logging.DEBUG if os.environ.get("XTYPE_DEBUG") else logging.INFO,
    format="%(asctime)s %(name)s %(levelname)s %(message)s",
)
log = logging.getLogger(__name__)

ENGINE_NAME = "xtype"


def main() -> None:
    IBus.init()
    bus = IBus.Bus()

    if not bus.is_connected():
        log.error("IBus daemon is not running — start with: ibus-daemon -drx")
        sys.exit(1)

    factory = IBus.Factory.new(bus.get_connection())
    factory.add_engine(ENGINE_NAME, CotypistEngine.__gtype__)

    if not bus.register_component(IBus.Component.new_from_file(
        os.path.join(os.path.dirname(__file__), "..", "cotypist.xml")
    )):
        log.warning("Failed to register component (daemon may have auto-loaded it)")

    bus.set_global_engine_async(ENGINE_NAME, -1, None, None, None)
    log.info("XType IBus engine started")

    IBus.main()


if __name__ == "__main__":
    main()
