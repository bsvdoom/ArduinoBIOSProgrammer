import contextlib
import hashlib
import importlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import serial

import xmodembios
from xmodem_operations import (
    ERASE_RESULT_TIMEOUT_SECONDS,
    ROM_SIZE_BYTES,
    STARTUP_RESET_DELAY_SECONDS,
    compare_files,
)
from xmodem_test_fakes import FakeModemFactory, FakeSerialFactory, no_sleep_factory


class CliParsingTests(unittest.TestCase):
    def test_main_help_lists_all_subcommands(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output), self.assertRaises(SystemExit) as raised:
            xmodembios.main(["--help"])
        self.assertEqual(raised.exception.code, 0)
        for command in ("read", "erase", "write", "verify"):
            self.assertIn(command, output.getvalue())

    def test_each_subcommand_has_help(self):
        for command in ("read", "erase", "write", "verify"):
            with self.subTest(command=command):
                output = io.StringIO()
                with contextlib.redirect_stdout(output), self.assertRaises(SystemExit) as raised:
                    xmodembios.main([command, "--help"])
                self.assertEqual(raised.exception.code, 0)
                self.assertIn("usage:", output.getvalue())

    def test_missing_command_is_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            xmodembios.main([])
        self.assertEqual(raised.exception.code, 2)

    def test_unknown_command_is_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            xmodembios.main(["unknown"])
        self.assertEqual(raised.exception.code, 2)

    def test_invalid_baud_is_usage_error_without_opening_port(self):
        serial_factory = FakeSerialFactory()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            xmodembios.main(
                ["erase", "COM3", "0"],
                serial_factory=serial_factory,
            )
        self.assertEqual(raised.exception.code, 2)
        self.assertEqual(serial_factory.calls, [])

    def test_import_has_no_serial_side_effect(self):
        sys.modules.pop("xmodembios", None)
        with patch("serial.Serial") as serial_constructor:
            importlib.import_module("xmodembios")
        serial_constructor.assert_not_called()


class CliOperationTests(unittest.TestCase):
    def run_cli(self, argv, serial_factory, modem_factory, expected_size):
        output = io.StringIO()
        error = io.StringIO()
        code = xmodembios.main(
            argv,
            serial_factory=serial_factory,
            modem_factory=modem_factory,
            sleeper=no_sleep_factory(serial_factory.events),
            expected_size=expected_size,
            output_stream=output,
            error_stream=error,
        )
        return code, output.getvalue(), error.getvalue()

    def test_default_port_baud_and_explicit_8n1_configuration(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        code, _, _ = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 0)
        settings = serial_factory.calls[0]
        self.assertEqual(settings["port"], "/dev/ttyUSB0")
        self.assertEqual(settings["baudrate"], 115200)
        self.assertEqual(settings["bytesize"], serial.EIGHTBITS)
        self.assertEqual(settings["parity"], serial.PARITY_NONE)
        self.assertEqual(settings["stopbits"], serial.STOPBITS_ONE)
        self.assertFalse(settings["xonxoff"])
        self.assertFalse(settings["rtscts"])
        self.assertFalse(settings["dsrdtr"])

    def test_explicit_linux_port_and_baud_are_unchanged(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        code, _, _ = self.run_cli(
            ["erase", "/dev/ttyUSB1", "57600"], serial_factory, modem_factory, 4
        )
        self.assertEqual(code, 0)
        self.assertEqual(serial_factory.calls[0]["port"], "/dev/ttyUSB1")
        self.assertEqual(serial_factory.calls[0]["baudrate"], 57600)

    def test_explicit_windows_port_is_unchanged(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        code, _, _ = self.run_cli(
            ["erase", "COM3", "115200"], serial_factory, modem_factory, 4
        )
        self.assertEqual(code, 0)
        self.assertEqual(serial_factory.calls[0]["port"], "COM3")

    def test_startup_wait_reset_and_initialize_order(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        code, _, _ = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 0)
        self.assertEqual(
            serial_factory.events[:4],
            [
                ("open", "/dev/ttyUSB0", 115200),
                ("sleep", STARTUP_RESET_DELAY_SECONDS),
                "reset_input_buffer",
                ("command", b"0"),
            ],
        )

    def test_port_open_failure_is_operation_error(self):
        serial_factory = FakeSerialFactory(open_error=True)
        modem_factory = FakeModemFactory(serial_factory)
        code, _, error = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 1)
        self.assertIn("port unavailable", error)

    def test_read_success_reports_hash_and_replaces_target(self):
        payload = bytes.fromhex("12 AF 34 56")
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, recv_payload=payload)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "dump.rom"
            target.write_bytes(b"old")
            code, output, error = self.run_cli(
                ["read", str(target)], serial_factory, modem_factory, len(payload)
            )
            self.assertEqual(target.read_bytes(), payload)
            self.assertFalse(Path(f"{target}.part").exists())
        self.assertEqual(code, 0)
        self.assertEqual(error, "")
        self.assertIn(hashlib.sha256(payload).hexdigest(), output)
        self.assertEqual(serial_factory.last.commands, [b"0", b"r"])
        self.assertTrue(serial_factory.last.closed)

    def test_read_00_through_ff_payload_is_unchanged(self):
        payload = bytes(range(256))
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, recv_payload=payload)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "dump.rom"
            code, _, _ = self.run_cli(
                ["read", str(target)], serial_factory, modem_factory, len(payload)
            )
            self.assertEqual(target.read_bytes(), payload)
        self.assertEqual(code, 0)

    def test_read_xmodem_failure_preserves_existing_target(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(
            serial_factory, recv_payload=b"part", recv_result=None
        )
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "dump.rom"
            target.write_bytes(b"known-good")
            code, _, error = self.run_cli(
                ["read", str(target)], serial_factory, modem_factory, 4
            )
            self.assertEqual(target.read_bytes(), b"known-good")
            self.assertFalse(Path(f"{target}.part").exists())
        self.assertEqual(code, 1)
        self.assertIn("XMODEM receive failed", error)
        self.assertTrue(serial_factory.last.closed)

    def test_read_short_result_is_rejected(self):
        self._assert_bad_read_size(b"abc", 4)

    def test_read_long_result_is_rejected(self):
        self._assert_bad_read_size(b"abcde", 4)

    def _assert_bad_read_size(self, payload, expected_size):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, recv_payload=payload)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "dump.rom"
            target.write_bytes(b"old")
            code, _, error = self.run_cli(
                ["read", str(target)], serial_factory, modem_factory, expected_size
            )
            self.assertEqual(target.read_bytes(), b"old")
            self.assertFalse(Path(f"{target}.part").exists())
        self.assertEqual(code, 1)
        self.assertIn("invalid received ROM size", error)

    def test_erase_success_sends_no_write_or_xmodem(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        code, output, error = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 0)
        self.assertEqual(error, "")
        self.assertEqual(serial_factory.last.commands, [b"0", b"e"])
        self.assertEqual(modem_factory.operations, [])
        self.assertIn("No write was started", output)
        self.assertTrue(serial_factory.last.closed)

    def test_erase_firmware_failure(self):
        serial_factory = FakeSerialFactory(erase_result="error")
        modem_factory = FakeModemFactory(serial_factory)
        code, _, error = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 1)
        self.assertIn("Erase failed", error)
        self.assertTrue(serial_factory.last.closed)

    def test_erase_timeout(self):
        serial_factory = FakeSerialFactory(erase_result="timeout")
        modem_factory = FakeModemFactory(serial_factory)
        code, _, error = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 1)
        self.assertIn("timed out", error)
        self.assertTrue(serial_factory.last.closed)

    def test_write_success_has_no_erase_or_verify(self):
        payload = bytes(range(32))
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(payload)
            code, output, error = self.run_cli(
                ["write", str(source)], serial_factory, modem_factory, len(payload)
            )
        self.assertEqual(code, 0)
        self.assertEqual(error, "")
        self.assertEqual(serial_factory.last.commands, [b"0", b"w"])
        self.assertEqual(modem_factory.operations, ["send"])
        self.assertNotIn(b"e", serial_factory.last.commands)
        self.assertNotIn(b"r", serial_factory.last.commands)
        self.assertEqual(modem_factory.sent_payload, payload)
        self.assertIn(hashlib.sha256(payload).hexdigest(), output)
        self.assertIn("Verification was not run", output)
        self.assertTrue(serial_factory.last.closed)

    def test_write_wrong_size_does_not_open_port(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(b"short")
            code, _, error = self.run_cli(
                ["write", str(source)], serial_factory, modem_factory, 8
            )
        self.assertEqual(code, 1)
        self.assertIn("invalid ROM size", error)
        self.assertEqual(serial_factory.calls, [])

    def test_write_missing_file_does_not_open_port(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.rom"
            code, _, error = self.run_cli(
                ["write", str(missing)], serial_factory, modem_factory, 4
            )
        self.assertEqual(code, 1)
        self.assertIn("not a regular file", error)
        self.assertEqual(serial_factory.calls, [])

    def test_write_send_false_is_failure(self):
        payload = b"data"
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, send_result=False)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(payload)
            code, _, error = self.run_cli(
                ["write", str(source)], serial_factory, modem_factory, len(payload)
            )
        self.assertEqual(code, 1)
        self.assertIn("XMODEM send failed", error)
        self.assertTrue(serial_factory.last.closed)

    def test_verify_matching_content_and_hashes(self):
        payload = bytes(range(64))
        code, output, error, serial_factory = self._verify(payload, payload)
        digest = hashlib.sha256(payload).hexdigest()
        self.assertEqual(code, 0)
        self.assertEqual(error, "")
        self.assertEqual(output.count(digest), 2)
        self.assertIn("Verify successful", output)
        self.assertEqual(serial_factory.last.commands, [b"0", b"r"])
        self.assertTrue(serial_factory.last.closed)

    def test_verify_first_byte_mismatch(self):
        code, _, error, _ = self._verify(b"abcd", b"xbcd")
        self.assertEqual(code, 3)
        self.assertIn("offset 0x0", error)
        self.assertIn("0x61", error)
        self.assertIn("0x78", error)

    def test_verify_last_byte_mismatch(self):
        code, _, error, _ = self._verify(b"abcd", b"abcx")
        self.assertEqual(code, 3)
        self.assertIn("offset 0x3", error)
        self.assertIn("0x64", error)
        self.assertIn("0x78", error)

    def test_verify_short_readback(self):
        code, _, error, _ = self._verify(b"abcd", b"abc")
        self.assertEqual(code, 3)
        self.assertIn("offset 0x3", error)
        self.assertIn("<EOF>", error)

    def test_verify_long_readback(self):
        code, _, error, _ = self._verify(b"abcd", b"abcde")
        self.assertEqual(code, 3)
        self.assertIn("offset 0x4", error)
        self.assertIn("<EOF>", error)

    def test_verify_xmodem_failure_is_operation_error_and_temp_is_removed(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "reference.rom"
            reference.write_bytes(b"abcd")
            serial_factory = FakeSerialFactory()
            modem_factory = FakeModemFactory(
                serial_factory, recv_payload=b"ab", recv_result=None
            )
            original_mkstemp = tempfile.mkstemp

            def local_mkstemp(*args, **kwargs):
                kwargs["dir"] = directory
                return original_mkstemp(*args, **kwargs)

            with patch("xmodem_operations.tempfile.mkstemp", side_effect=local_mkstemp):
                code, _, error = self.run_cli(
                    ["verify", str(reference)], serial_factory, modem_factory, 4
                )
            self.assertEqual(list(Path(directory).glob("xmodembios-verify-*.part")), [])
        self.assertEqual(code, 1)
        self.assertIn("XMODEM receive failed", error)
        self.assertTrue(serial_factory.last.closed)

    def test_verify_wrong_reference_size_does_not_open_port(self):
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "reference.rom"
            reference.write_bytes(b"short")
            code, _, error = self.run_cli(
                ["verify", str(reference)], serial_factory, modem_factory, 8
            )
        self.assertEqual(code, 1)
        self.assertIn("invalid ROM size", error)
        self.assertEqual(serial_factory.calls, [])

    def test_verify_interrupt_returns_130_closes_port_and_removes_temp(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "reference.rom"
            reference.write_bytes(b"abcd")
            serial_factory = FakeSerialFactory()
            modem_factory = FakeModemFactory(
                serial_factory, recv_payload=b"ab", interrupt_recv=True
            )
            original_mkstemp = tempfile.mkstemp

            def local_mkstemp(*args, **kwargs):
                kwargs["dir"] = directory
                return original_mkstemp(*args, **kwargs)

            with patch("xmodem_operations.tempfile.mkstemp", side_effect=local_mkstemp):
                code, _, error = self.run_cli(
                    ["verify", str(reference)], serial_factory, modem_factory, 4
                )
            self.assertEqual(list(Path(directory).glob("xmodembios-verify-*.part")), [])
        self.assertEqual(code, 130)
        self.assertIn("interrupted", error)
        self.assertTrue(serial_factory.last.closed)

    def test_invalid_firmware_utf8_is_protocol_failure(self):
        serial_factory = FakeSerialFactory(invalid_status=True)
        modem_factory = FakeModemFactory(serial_factory)
        code, _, error = self.run_cli(["erase"], serial_factory, modem_factory, 4)
        self.assertEqual(code, 1)
        self.assertIn("decode failed", error)

    def test_payload_patterns_remain_byte_exact_without_pair_swap(self):
        for payload in (bytes.fromhex("12 AF 34 56"), bytes(range(256))):
            with self.subTest(length=len(payload)):
                serial_factory = FakeSerialFactory()
                modem_factory = FakeModemFactory(serial_factory)
                with tempfile.TemporaryDirectory() as directory:
                    source = Path(directory) / "input.rom"
                    source.write_bytes(payload)
                    code, _, _ = self.run_cli(
                        ["write", str(source)],
                        serial_factory,
                        modem_factory,
                        len(payload),
                    )
                self.assertEqual(code, 0)
                self.assertEqual(modem_factory.sent_payload, payload)
        self.assertNotEqual(bytes.fromhex("12 AF 34 56"), bytes.fromhex("AF 12 56 34"))

    def test_production_rom_size_constant_is_exactly_16_mib(self):
        self.assertEqual(ROM_SIZE_BYTES, 16_777_216)

    def test_erase_host_timeout_exceeds_firmware_timeout(self):
        self.assertGreater(ERASE_RESULT_TIMEOUT_SECONDS, 250.0)

    def test_compare_files_finds_difference_across_small_blocks(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "reference.rom"
            actual = Path(directory) / "actual.rom"
            reference.write_bytes(b"abcdef")
            actual.write_bytes(b"abcdXf")
            result = compare_files(reference, actual, block_size=2)
        self.assertFalse(result.matches)
        self.assertEqual(result.first_offset, 4)
        self.assertEqual(result.reference_byte, ord("e"))
        self.assertEqual(result.actual_byte, ord("X"))

    def _verify(self, reference_payload, actual_payload):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "reference.rom"
            reference.write_bytes(reference_payload)
            serial_factory = FakeSerialFactory()
            modem_factory = FakeModemFactory(serial_factory, recv_payload=actual_payload)
            code, output, error = self.run_cli(
                ["verify", str(reference)],
                serial_factory,
                modem_factory,
                len(reference_payload),
            )
        return code, output, error, serial_factory


if __name__ == "__main__":
    unittest.main()
