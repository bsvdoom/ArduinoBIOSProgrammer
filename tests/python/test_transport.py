import unittest

from serial import SerialException, SerialTimeoutException

from xmodem_transport import SerialTransport


class FakeClock:
    def __init__(self, *values):
        self.values = list(values) or [0.0]
        self.last = self.values[-1]

    def __call__(self):
        if self.values:
            self.last = self.values.pop(0)
        return self.last


class FakeSerial:
    def __init__(self, reads=(), write_counts=()):
        self.timeout = 17.0
        self.write_timeout = 19.0
        self.reads = list(reads)
        self.write_counts = list(write_counts)
        self.read_requests = []
        self.write_requests = []

    def read(self, size):
        self.read_requests.append((size, self.timeout))
        value = self.reads.pop(0) if self.reads else b""
        if isinstance(value, BaseException):
            raise value
        return value

    def write(self, data):
        self.write_requests.append((bytes(data), self.write_timeout))
        value = self.write_counts.pop(0) if self.write_counts else len(data)
        if isinstance(value, BaseException):
            raise value
        return value


class SerialTransportTests(unittest.TestCase):
    def test_getc_full_read(self):
        serial_port = FakeSerial(reads=[b"abcd"])
        result = SerialTransport(serial_port, FakeClock(0.0)).getc(4, 2)
        self.assertEqual(result, b"abcd")
        self.assertEqual(serial_port.read_requests[0][0], 4)

    def test_getc_collects_partial_reads(self):
        serial_port = FakeSerial(reads=[b"ab", b"cd"])
        result = SerialTransport(serial_port, FakeClock(0.0)).getc(4, 2)
        self.assertEqual(result, b"abcd")
        self.assertEqual([request[0] for request in serial_port.read_requests], [4, 2])

    def test_getc_returns_partial_data_at_deadline(self):
        serial_port = FakeSerial(reads=[b"ab"])
        result = SerialTransport(serial_port, FakeClock(0.0, 0.0, 2.0)).getc(4, 2)
        self.assertEqual(result, b"ab")

    def test_getc_empty_timeout_returns_none(self):
        serial_port = FakeSerial(reads=[b""])
        self.assertIsNone(SerialTransport(serial_port, FakeClock(0.0)).getc(1, 1))

    def test_getc_restores_timeout(self):
        serial_port = FakeSerial(reads=[b"x"])
        SerialTransport(serial_port, FakeClock(0.0)).getc(1, 3)
        self.assertEqual(serial_port.timeout, 17.0)
        self.assertLessEqual(serial_port.read_requests[0][1], 3.0)

    def test_getc_restores_timeout_after_serial_error(self):
        serial_port = FakeSerial(reads=[SerialException("read failed")])
        with self.assertRaises(SerialException):
            SerialTransport(serial_port, FakeClock(0.0)).getc(1, 3)
        self.assertEqual(serial_port.timeout, 17.0)

    def test_putc_full_write(self):
        serial_port = FakeSerial()
        result = SerialTransport(serial_port, FakeClock(0.0)).putc(b"abcd", 2)
        self.assertEqual(result, 4)
        self.assertEqual(serial_port.write_requests[0][0], b"abcd")

    def test_putc_completes_multiple_partial_writes(self):
        serial_port = FakeSerial(write_counts=[2, 2])
        result = SerialTransport(serial_port, FakeClock(0.0)).putc(b"abcd", 2)
        self.assertEqual(result, 4)
        self.assertEqual([item[0] for item in serial_port.write_requests], [b"abcd", b"cd"])

    def test_putc_zero_progress_returns_none(self):
        serial_port = FakeSerial(write_counts=[0])
        self.assertIsNone(SerialTransport(serial_port, FakeClock(0.0)).putc(b"x", 1))

    def test_putc_deadline_after_partial_write_returns_none(self):
        serial_port = FakeSerial(write_counts=[1])
        result = SerialTransport(serial_port, FakeClock(0.0, 0.0, 2.0)).putc(b"xy", 2)
        self.assertIsNone(result)

    def test_putc_serial_timeout_returns_none(self):
        serial_port = FakeSerial(write_counts=[SerialTimeoutException("timeout")])
        result = SerialTransport(serial_port, FakeClock(0.0)).putc(b"x", 1)
        self.assertIsNone(result)

    def test_putc_restores_write_timeout(self):
        serial_port = FakeSerial()
        SerialTransport(serial_port, FakeClock(0.0)).putc(b"x", 4)
        self.assertEqual(serial_port.write_timeout, 19.0)
        self.assertLessEqual(serial_port.write_requests[0][1], 4.0)

    def test_putc_rejects_text(self):
        with self.assertRaises(TypeError):
            SerialTransport(FakeSerial()).putc("not bytes")


if __name__ == "__main__":
    unittest.main()
