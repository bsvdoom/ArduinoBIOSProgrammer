import hashlib
import importlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from xmodem_test_fakes import FakeModemFactory, FakeSerialFactory, no_sleep_factory


class ImportTests(unittest.TestCase):
    def _assert_import_has_no_serial_open(self, module_name):
        sys.modules.pop(module_name, None)
        with patch("serial.Serial") as serial_constructor:
            importlib.import_module(module_name)
        serial_constructor.assert_not_called()

    def test_read_module_import_has_no_side_effect(self):
        self._assert_import_has_no_serial_open("xmodembiosread")

    def test_write_module_import_has_no_side_effect(self):
        self._assert_import_has_no_serial_open("xmodembioswrite")


class LegacyReadTests(unittest.TestCase):
    def test_reader_uses_shared_read_operation(self):
        module = importlib.import_module("xmodembiosread")
        payload = bytes.fromhex("12 AF 34 56")
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, recv_payload=payload)
        output = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "output.rom"
            code = module.main(
                serial_factory=serial_factory,
                modem_factory=modem_factory,
                output_stream=output,
                error_stream=io.StringIO(),
                output_path=target,
                expected_size=len(payload),
                sleeper=no_sleep_factory(serial_factory.events),
            )
            self.assertEqual(target.read_bytes(), payload)
        self.assertEqual(code, 0)
        self.assertEqual(serial_factory.last.commands, [b"0", b"r"])
        self.assertIn(hashlib.sha256(payload).hexdigest(), output.getvalue())
        self.assertTrue(serial_factory.last.closed)

    def test_reader_failure_preserves_existing_target(self):
        module = importlib.import_module("xmodembiosread")
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, recv_payload=b"x", recv_result=None)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "output.rom"
            target.write_bytes(b"old")
            code = module.main(
                serial_factory=serial_factory,
                modem_factory=modem_factory,
                output_stream=io.StringIO(),
                error_stream=io.StringIO(),
                output_path=target,
                expected_size=1,
                sleeper=no_sleep_factory(serial_factory.events),
            )
            self.assertEqual(target.read_bytes(), b"old")
            self.assertFalse(Path(f"{target}.part").exists())
        self.assertEqual(code, 1)
        self.assertTrue(serial_factory.last.closed)


class LegacyWriteTests(unittest.TestCase):
    def test_writer_uses_shared_write_operation(self):
        module = importlib.import_module("xmodembioswrite")
        payload = bytes(range(256))
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        output = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(payload)
            code = module.main(
                serial_factory=serial_factory,
                modem_factory=modem_factory,
                output_stream=output,
                error_stream=io.StringIO(),
                input_path=source,
                expected_size=len(payload),
                sleeper=no_sleep_factory(serial_factory.events),
            )
        self.assertEqual(code, 0)
        self.assertEqual(serial_factory.last.commands, [b"0", b"w"])
        self.assertEqual(modem_factory.sent_payload, payload)
        self.assertIn("Verification was not run", output.getvalue())
        self.assertTrue(serial_factory.last.closed)

    def test_writer_false_send_is_nonzero(self):
        module = importlib.import_module("xmodembioswrite")
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory, send_result=False)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(b"data")
            code = module.main(
                serial_factory=serial_factory,
                modem_factory=modem_factory,
                output_stream=io.StringIO(),
                error_stream=io.StringIO(),
                input_path=source,
                expected_size=4,
                sleeper=no_sleep_factory(serial_factory.events),
            )
        self.assertEqual(code, 1)
        self.assertTrue(serial_factory.last.closed)

    def test_writer_bad_size_does_not_open_port(self):
        module = importlib.import_module("xmodembioswrite")
        serial_factory = FakeSerialFactory()
        modem_factory = FakeModemFactory(serial_factory)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.rom"
            source.write_bytes(b"short")
            code = module.main(
                serial_factory=serial_factory,
                modem_factory=modem_factory,
                output_stream=io.StringIO(),
                error_stream=io.StringIO(),
                input_path=source,
                expected_size=8,
                sleeper=no_sleep_factory(serial_factory.events),
            )
        self.assertEqual(code, 1)
        self.assertEqual(serial_factory.calls, [])


if __name__ == "__main__":
    unittest.main()
