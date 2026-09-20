"""Timeout-aware pySerial callbacks for the xmodem package."""

from __future__ import annotations

import time
from collections.abc import Callable

from serial import SerialTimeoutException


class SerialTransport:
    """Adapt a pySerial-compatible object to XMODEM's callback contract."""

    def __init__(self, serial_port, monotonic: Callable[[], float] = time.monotonic):
        self._serial = serial_port
        self._monotonic = monotonic

    def getc(self, size: int, timeout: float = 1) -> bytes | None:
        """Read up to *size* bytes before one shared timeout expires.

        A short pySerial read is accumulated while time remains. If the
        deadline expires after some data arrived, those bytes are returned;
        an entirely empty timeout is represented by ``None``.
        """
        if size < 0:
            raise ValueError("size must not be negative")
        if size == 0:
            return b""

        timeout = max(0.0, float(timeout))
        deadline = self._monotonic() + timeout
        previous_timeout = self._serial.timeout
        chunks = bytearray()

        try:
            while len(chunks) < size:
                remaining = max(0.0, deadline - self._monotonic())
                self._serial.timeout = remaining
                chunk = self._serial.read(size - len(chunks))
                if not isinstance(chunk, (bytes, bytearray, memoryview)):
                    raise TypeError("serial read must return bytes-like data")
                if chunk:
                    chunks.extend(chunk)
                    if len(chunks) >= size:
                        break
                else:
                    break
                if self._monotonic() >= deadline:
                    break
        finally:
            self._serial.timeout = previous_timeout

        return bytes(chunks) if chunks else None

    def putc(self, data, timeout: float = 1) -> int | None:
        """Write all bytes before one shared timeout expires.

        Returns the complete byte count on success and ``None`` for a timeout,
        a zero-progress write, or ``SerialTimeoutException``.
        """
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("serial write requires bytes-like data")

        payload = bytes(data)
        if not payload:
            return 0

        timeout = max(0.0, float(timeout))
        deadline = self._monotonic() + timeout
        previous_timeout = self._serial.write_timeout
        written = 0

        try:
            while written < len(payload):
                remaining = max(0.0, deadline - self._monotonic())
                self._serial.write_timeout = remaining
                try:
                    count = self._serial.write(payload[written:])
                except SerialTimeoutException:
                    return None

                if count is None or count <= 0:
                    return None
                if count > len(payload) - written:
                    raise ValueError("serial write returned an invalid byte count")
                written += count
                if written < len(payload) and self._monotonic() >= deadline:
                    return None
        finally:
            self._serial.write_timeout = previous_timeout

        return written
