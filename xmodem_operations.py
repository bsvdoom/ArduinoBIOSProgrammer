"""Shared BIOS programmer operations used by the CLI and legacy wrappers."""

from __future__ import annotations

import hashlib
import os
import tempfile
import time
from contextlib import AbstractContextManager
from dataclasses import dataclass
from pathlib import Path

import serial
from xmodem import XMODEM

from xmodem_transport import SerialTransport


DEFAULT_SERIAL_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD_RATE = 115200
ROM_SIZE_BYTES = 16_777_216

STARTUP_RESET_DELAY_SECONDS = 2.0
COMMAND_WRITE_TIMEOUT_SECONDS = 2.0
INITIALIZE_STATUS_TIMEOUT_SECONDS = 10.0
TRANSFER_PROMPT_TIMEOUT_SECONDS = 5.0
WRITE_RESULT_TIMEOUT_SECONDS = 10.0
ERASE_RESULT_TIMEOUT_SECONDS = 300.0
HASH_BLOCK_SIZE = 64 * 1024

INITIALIZE_COMMAND = b"0"
READ_COMMAND = b"r"
ERASE_COMMAND = b"e"
WRITE_COMMAND = b"w"


class OperationError(Exception):
    """Expected serial, protocol, transfer, or file operation failure."""


class ProtocolTimeout(OperationError):
    """A finite firmware status or command deadline expired."""


class FirmwareError(OperationError):
    """The firmware returned one of its documented error messages."""


@dataclass(frozen=True)
class FileInfo:
    size: int
    sha256: str


@dataclass(frozen=True)
class VerifyResult:
    matches: bool
    reference: FileInfo
    actual: FileInfo
    first_offset: int | None = None
    reference_byte: int | None = None
    actual_byte: int | None = None


FIRMWARE_ERROR_MARKERS = (
    "INITIALIZE FAILED",
    "Chip is not initialized",
    "Erase failed",
    "Flash write failed",
    "Failed to start xmodem",
    "Out of memory",
    "Invalid file size",
    "Timeout",
    "Cancelled by other party",
    "Unknown result",
    "Flash read failed",
    "Transfer cancelled",
    "Transfer error",
    "Unknown command code",
)


def _noop_status(_line: str) -> None:
    pass


class SerialSession(AbstractContextManager):
    """Own one configured serial connection and its firmware status protocol."""

    def __init__(
        self,
        port: str,
        baud_rate: int,
        *,
        serial_factory=serial.Serial,
        modem_factory=XMODEM,
        sleeper=time.sleep,
        monotonic=time.monotonic,
        status_callback=_noop_status,
    ):
        self.port = port
        self.baud_rate = baud_rate
        self.serial_factory = serial_factory
        self.modem_factory = modem_factory
        self.sleeper = sleeper
        self.monotonic = monotonic
        self.status_callback = status_callback
        self.serial_port = None
        self.transport = None

    def __enter__(self):
        try:
            self.serial_port = self.serial_factory(
                port=self.port,
                baudrate=self.baud_rate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0,
                write_timeout=COMMAND_WRITE_TIMEOUT_SECONDS,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False,
            )
            self.sleeper(STARTUP_RESET_DELAY_SECONDS)
            self.serial_port.reset_input_buffer()
            self.transport = SerialTransport(self.serial_port, self.monotonic)
            return self
        except BaseException:
            if self.serial_port is not None:
                self.serial_port.close()
            raise

    def __exit__(self, exc_type, exc_value, traceback):
        if self.serial_port is not None:
            self.serial_port.close()
        return False

    def initialize(self) -> None:
        self._send_command(INITIALIZE_COMMAND)
        ready = False

        def initialized(line: str) -> bool:
            nonlocal ready
            if line == "CHIP READY":
                ready = True
            return ready and line.startswith("Flash size:")

        self._wait_for(initialized, INITIALIZE_STATUS_TIMEOUT_SECONDS, "initialization")

    def prepare_read(self) -> None:
        self._send_command(READ_COMMAND)
        self._wait_for(
            lambda line: line == "Sending file using xmodem, please start receive",
            TRANSFER_PROMPT_TIMEOUT_SECONDS,
            "read prompt",
        )

    def prepare_write(self) -> None:
        self._send_command(WRITE_COMMAND)
        self._wait_for(
            lambda line: line == "Ready to receive file using xmodem, please start send",
            TRANSFER_PROMPT_TIMEOUT_SECONDS,
            "write prompt",
        )

    def wait_for_write_result(self) -> None:
        self._wait_for(
            lambda line: line == "Write done",
            WRITE_RESULT_TIMEOUT_SECONDS,
            "write result",
        )

    def erase(self) -> None:
        self._send_command(ERASE_COMMAND)
        self._wait_for(
            lambda line: line == "Done",
            ERASE_RESULT_TIMEOUT_SECONDS,
            "chip erase",
        )

    def modem(self):
        return self.modem_factory(self.transport.getc, self.transport.putc)

    def _send_command(self, command: bytes) -> None:
        written = self.transport.putc(command, COMMAND_WRITE_TIMEOUT_SECONDS)
        if written != len(command):
            raise ProtocolTimeout(f"firmware command {command!r} write timed out")

    def _wait_for(self, success, timeout: float, context: str) -> None:
        deadline = self.monotonic() + timeout
        while True:
            remaining = deadline - self.monotonic()
            if remaining <= 0:
                raise ProtocolTimeout(f"timed out waiting for firmware {context}")

            line = self._read_status_line(remaining, context)
            self.status_callback(line)
            if any(marker.casefold() in line.casefold() for marker in FIRMWARE_ERROR_MARKERS):
                raise FirmwareError(f"firmware error during {context}: {line}")
            if success(line):
                return

    def _read_status_line(self, timeout: float, context: str) -> str:
        previous_timeout = self.serial_port.timeout
        try:
            self.serial_port.timeout = max(0.0, timeout)
            raw = self.serial_port.readline()
        finally:
            self.serial_port.timeout = previous_timeout

        if not raw:
            raise ProtocolTimeout(f"timed out waiting for firmware {context}")
        if not isinstance(raw, (bytes, bytearray, memoryview)):
            raise OperationError("firmware status must be bytes-like")
        raw = bytes(raw)
        if not raw.endswith(b"\n"):
            raise ProtocolTimeout(f"unterminated firmware status during {context}")
        try:
            return raw.decode("utf-8").rstrip("\r\n")
        except UnicodeDecodeError as error:
            raise OperationError(f"firmware status decode failed during {context}") from error


def _session(
    port,
    baud_rate,
    *,
    serial_factory,
    modem_factory,
    sleeper,
    monotonic,
    status_callback,
):
    return SerialSession(
        port,
        baud_rate,
        serial_factory=serial_factory,
        modem_factory=modem_factory,
        sleeper=sleeper,
        monotonic=monotonic,
        status_callback=status_callback,
    )


def _receive_to_path(session: SerialSession, path: Path) -> int:
    session.prepare_read()
    with path.open("wb") as stream:
        received = session.modem().recv(stream)
        if received is None:
            raise OperationError("XMODEM receive failed")
        stream.flush()
        os.fsync(stream.fileno())
    return path.stat().st_size


def file_info(path: Path, block_size: int = HASH_BLOCK_SIZE) -> FileInfo:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(block_size)
            if not chunk:
                break
            size += len(chunk)
            digest.update(chunk)
    return FileInfo(size=size, sha256=digest.hexdigest())


def validate_rom(path, expected_size: int = ROM_SIZE_BYTES) -> FileInfo:
    path = Path(path)
    if not path.is_file():
        raise OperationError(f"file is missing or not a regular file: {path}")
    info = file_info(path)
    if info.size != expected_size:
        raise OperationError(
            f"invalid ROM size: {info.size} bytes; expected {expected_size} bytes"
        )
    return info


def read_rom(
    target,
    port=DEFAULT_SERIAL_PORT,
    baud_rate=DEFAULT_BAUD_RATE,
    *,
    expected_size=ROM_SIZE_BYTES,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    sleeper=time.sleep,
    monotonic=time.monotonic,
    status_callback=_noop_status,
) -> FileInfo:
    target = Path(target)
    part_path = target.with_name(target.name + ".part")
    completed = False
    try:
        with _session(
            port,
            baud_rate,
            serial_factory=serial_factory,
            modem_factory=modem_factory,
            sleeper=sleeper,
            monotonic=monotonic,
            status_callback=status_callback,
        ) as session:
            session.initialize()
            actual_size = _receive_to_path(session, part_path)
        if actual_size != expected_size:
            raise OperationError(
                f"invalid received ROM size: {actual_size} bytes; expected {expected_size} bytes"
            )
        info = file_info(part_path)
        os.replace(part_path, target)
        completed = True
        return info
    finally:
        if not completed:
            try:
                part_path.unlink()
            except FileNotFoundError:
                pass


def erase_chip(
    port=DEFAULT_SERIAL_PORT,
    baud_rate=DEFAULT_BAUD_RATE,
    *,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    sleeper=time.sleep,
    monotonic=time.monotonic,
    status_callback=_noop_status,
) -> None:
    with _session(
        port,
        baud_rate,
        serial_factory=serial_factory,
        modem_factory=modem_factory,
        sleeper=sleeper,
        monotonic=monotonic,
        status_callback=status_callback,
    ) as session:
        session.initialize()
        session.erase()


def write_rom(
    source,
    port=DEFAULT_SERIAL_PORT,
    baud_rate=DEFAULT_BAUD_RATE,
    *,
    expected_size=ROM_SIZE_BYTES,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    sleeper=time.sleep,
    monotonic=time.monotonic,
    status_callback=_noop_status,
) -> FileInfo:
    source = Path(source)
    info = validate_rom(source, expected_size)
    with source.open("rb") as stream:
        with _session(
            port,
            baud_rate,
            serial_factory=serial_factory,
            modem_factory=modem_factory,
            sleeper=sleeper,
            monotonic=monotonic,
            status_callback=status_callback,
        ) as session:
            session.initialize()
            session.prepare_write()
            if not session.modem().send(stream):
                raise OperationError("XMODEM send failed")
            session.wait_for_write_result()
    return info


def compare_files(reference, actual, block_size: int = HASH_BLOCK_SIZE) -> VerifyResult:
    reference = Path(reference)
    actual = Path(actual)
    reference_hash = hashlib.sha256()
    actual_hash = hashlib.sha256()
    reference_size = 0
    actual_size = 0
    first_offset = None
    reference_byte = None
    actual_byte = None
    position = 0

    with reference.open("rb") as reference_stream, actual.open("rb") as actual_stream:
        while True:
            reference_chunk = reference_stream.read(block_size)
            actual_chunk = actual_stream.read(block_size)
            if not reference_chunk and not actual_chunk:
                break

            reference_hash.update(reference_chunk)
            actual_hash.update(actual_chunk)
            reference_size += len(reference_chunk)
            actual_size += len(actual_chunk)

            if first_offset is None:
                shared = min(len(reference_chunk), len(actual_chunk))
                for index in range(shared):
                    if reference_chunk[index] != actual_chunk[index]:
                        first_offset = position + index
                        reference_byte = reference_chunk[index]
                        actual_byte = actual_chunk[index]
                        break
                if first_offset is None and len(reference_chunk) != len(actual_chunk):
                    first_offset = position + shared
                    reference_byte = reference_chunk[shared] if shared < len(reference_chunk) else None
                    actual_byte = actual_chunk[shared] if shared < len(actual_chunk) else None
            position += min(len(reference_chunk), len(actual_chunk))

    reference_info = FileInfo(reference_size, reference_hash.hexdigest())
    actual_info = FileInfo(actual_size, actual_hash.hexdigest())
    return VerifyResult(
        matches=first_offset is None,
        reference=reference_info,
        actual=actual_info,
        first_offset=first_offset,
        reference_byte=reference_byte,
        actual_byte=actual_byte,
    )


def verify_rom(
    reference,
    port=DEFAULT_SERIAL_PORT,
    baud_rate=DEFAULT_BAUD_RATE,
    *,
    expected_size=ROM_SIZE_BYTES,
    serial_factory=serial.Serial,
    modem_factory=XMODEM,
    sleeper=time.sleep,
    monotonic=time.monotonic,
    status_callback=_noop_status,
) -> VerifyResult:
    reference = Path(reference)
    validate_rom(reference, expected_size)
    descriptor, temporary_name = tempfile.mkstemp(prefix="xmodembios-verify-", suffix=".part")
    os.close(descriptor)
    temporary_path = Path(temporary_name)
    try:
        with _session(
            port,
            baud_rate,
            serial_factory=serial_factory,
            modem_factory=modem_factory,
            sleeper=sleeper,
            monotonic=monotonic,
            status_callback=status_callback,
        ) as session:
            session.initialize()
            _receive_to_path(session, temporary_path)
        return compare_files(reference, temporary_path)
    finally:
        try:
            temporary_path.unlink()
        except FileNotFoundError:
            pass
