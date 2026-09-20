#!/usr/bin/env python3
"""Unified command-line interface for the Arduino BIOS programmer."""

from __future__ import annotations

import argparse
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
    erase_chip,
    read_rom,
    verify_rom,
    write_rom,
)


EXIT_SUCCESS = 0
EXIT_OPERATION_ERROR = 1
EXIT_USAGE_ERROR = 2
EXIT_VERIFY_MISMATCH = 3
EXIT_INTERRUPTED = 130


def positive_baud(value: str) -> int:
    try:
        baud_rate = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("baud_rate must be a positive integer") from error
    if baud_rate <= 0:
        raise argparse.ArgumentTypeError("baud_rate must be a positive integer")
    return baud_rate


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Read, erase, write, or verify a 16 MiB SPI flash through Arduino XMODEM.",
        epilog="Erase, write, and verify are separate explicit operations; write runs neither automatically.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    def add_connection_arguments(subparser):
        subparser.add_argument(
            "serial_port",
            nargs="?",
            default=DEFAULT_SERIAL_PORT,
            help=f"serial port, for example /dev/ttyUSB0 or COM3 (default: {DEFAULT_SERIAL_PORT})",
        )
        subparser.add_argument(
            "baud_rate",
            nargs="?",
            default=DEFAULT_BAUD_RATE,
            type=positive_baud,
            help=f"positive serial baud rate (default: {DEFAULT_BAUD_RATE})",
        )

    read_parser = subparsers.add_parser("read", help="read the complete flash into a ROM file")
    read_parser.add_argument("file", help="destination ROM file")
    add_connection_arguments(read_parser)

    erase_parser = subparsers.add_parser("erase", help="erase the complete flash; does not write")
    add_connection_arguments(erase_parser)

    write_parser = subparsers.add_parser("write", help="write a ROM; does not erase or verify")
    write_parser.add_argument("file", help="exactly 16 MiB input ROM file")
    add_connection_arguments(write_parser)

    verify_parser = subparsers.add_parser(
        "verify", help="read the complete flash and compare it with a ROM file"
    )
    verify_parser.add_argument("file", help="exactly 16 MiB reference ROM file")
    add_connection_arguments(verify_parser)
    return parser


def _format_byte(value: int | None) -> str:
    return "<EOF>" if value is None else f"0x{value:02X}"


def main(
    argv=None,
    *,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    sleeper=time.sleep,
    monotonic=time.monotonic,
    expected_size=ROM_SIZE_BYTES,
    output_stream=None,
    error_stream=None,
) -> int:
    output_stream = sys.stdout if output_stream is None else output_stream
    error_stream = sys.stderr if error_stream is None else error_stream
    arguments = build_parser().parse_args(argv)

    def status(line: str) -> None:
        print(line, file=output_stream)

    common = {
        "serial_factory": serial_factory,
        "modem_factory": modem_factory,
        "sleeper": sleeper,
        "monotonic": monotonic,
        "status_callback": status,
    }

    try:
        if arguments.command == "read":
            info = read_rom(
                arguments.file,
                arguments.serial_port,
                arguments.baud_rate,
                expected_size=expected_size,
                **common,
            )
            print(
                f"Read complete: {info.size} bytes, SHA-256 {info.sha256}",
                file=output_stream,
            )
            return EXIT_SUCCESS

        if arguments.command == "erase":
            print("Erase is a separate operation; no write will be started.", file=output_stream)
            erase_chip(arguments.serial_port, arguments.baud_rate, **common)
            print("Erase complete. No write was started.", file=output_stream)
            return EXIT_SUCCESS

        if arguments.command == "write":
            print(
                "Write is separate from erase and verify; neither runs automatically.",
                file=output_stream,
            )
            info = write_rom(
                arguments.file,
                arguments.serial_port,
                arguments.baud_rate,
                expected_size=expected_size,
                **common,
            )
            print(
                f"Write complete: {info.size} bytes, SHA-256 {info.sha256}. "
                "Verification was not run.",
                file=output_stream,
            )
            return EXIT_SUCCESS

        result = verify_rom(
            arguments.file,
            arguments.serial_port,
            arguments.baud_rate,
            expected_size=expected_size,
            **common,
        )
        print(
            f"Reference: {result.reference.size} bytes, SHA-256 {result.reference.sha256}",
            file=output_stream,
        )
        print(
            f"Readback: {result.actual.size} bytes, SHA-256 {result.actual.sha256}",
            file=output_stream,
        )
        if result.matches:
            print("Verify successful: flash content matches the reference.", file=output_stream)
            return EXIT_SUCCESS
        print(
            "Verify mismatch at offset "
            f"0x{result.first_offset:X}: reference {_format_byte(result.reference_byte)}, "
            f"readback {_format_byte(result.actual_byte)}",
            file=error_stream,
        )
        return EXIT_VERIFY_MISMATCH
    except KeyboardInterrupt:
        print("Error: operation interrupted by user.", file=error_stream)
        return EXIT_INTERRUPTED
    except (OperationError, SerialTimeoutException, SerialException, OSError) as error:
        print(f"Error: {error}", file=error_stream)
        return EXIT_OPERATION_ERROR


if __name__ == "__main__":
    raise SystemExit(main())
