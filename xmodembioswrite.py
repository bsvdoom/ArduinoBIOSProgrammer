"""Legacy standalone writer backed by the shared Python 3 operations."""

from __future__ import annotations

import sys
import time

import serial
from serial import SerialException, SerialTimeoutException
from xmodem import XMODEM

from xmodem_operations import (
    DEFAULT_BAUD_RATE,
    DEFAULT_SERIAL_PORT,
    ROM_SIZE_BYTES,
    OperationError,
    write_rom,
)


SERIAL_PORT = DEFAULT_SERIAL_PORT
BAUD_RATE = DEFAULT_BAUD_RATE
INPUT_FILENAME = "input.rom"


def main(
    *,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    input_stream=None,
    output_stream=None,
    error_stream=None,
    input_path=INPUT_FILENAME,
    expected_size=ROM_SIZE_BYTES,
    sleeper=time.sleep,
    monotonic=time.monotonic,
) -> int:
    del input_stream  # Kept as an injected compatibility argument; no manual wait remains.
    output_stream = sys.stdout if output_stream is None else output_stream
    error_stream = sys.stderr if error_stream is None else error_stream

    try:
        print(
            "Write does not erase or verify automatically.",
            file=output_stream,
        )
        info = write_rom(
            input_path,
            SERIAL_PORT,
            BAUD_RATE,
            expected_size=expected_size,
            serial_factory=serial_factory,
            modem_factory=modem_factory,
            sleeper=sleeper,
            monotonic=monotonic,
            status_callback=lambda line: print(line, file=output_stream),
        )
        print(
            f"Write complete: {info.size} bytes, SHA-256 {info.sha256}. "
            "Verification was not run.",
            file=output_stream,
        )
        return 0
    except KeyboardInterrupt:
        print("Error: write interrupted.", file=error_stream)
        return 130
    except (OperationError, SerialTimeoutException, SerialException, OSError) as error:
        print(f"Error: {error}", file=error_stream)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
