from serial import SerialException


INITIALIZE_SUCCESS = [
    b"\x1b[0m\x1b[2JInitializing Chip...\r\n",
    b"CHIP READY\r\n",
    b"Type: not implemented\r\n",
    b"Flash size: 16777216b (16384k / 16M)\r\n",
]
READ_PROMPT = [
    b"Read chip\r\n",
    b"Sending file using xmodem, please start receive\r\n",
]
WRITE_PROMPT = [
    b"Write chip\r\n",
    b"Ready to receive file using xmodem, please start send\r\n",
]


class FakeSerial:
    def __init__(self, factory, kwargs):
        self.factory = factory
        self.kwargs = kwargs
        self.timeout = kwargs["timeout"]
        self.write_timeout = kwargs["write_timeout"]
        self.lines = [b"stale startup bytes\r\n"]
        self.closed = False
        self.commands = []

    def reset_input_buffer(self):
        self.factory.events.append("reset_input_buffer")
        self.lines.clear()

    def read(self, size):
        self.factory.events.append(("read", size))
        return b""

    def write(self, data):
        command = bytes(data)
        self.commands.append(command)
        self.factory.events.append(("command", command))
        if command == b"0":
            if self.factory.initialize_error:
                self.lines.extend(
                    [b"\x1b[0m\x1b[2JInitializing Chip...\r\n", b"INITIALIZE FAILED\r\n"]
                )
            elif self.factory.invalid_status:
                self.lines.append(b"\xff\n")
            else:
                self.lines.extend(INITIALIZE_SUCCESS)
        elif command == b"r":
            self.lines.extend(READ_PROMPT)
        elif command == b"w":
            self.lines.extend(WRITE_PROMPT)
        elif command == b"e":
            self.lines.append(b"Erase all\r\n")
            if self.factory.erase_result == "success":
                self.lines.append(b"Done\r\n")
            elif self.factory.erase_result == "error":
                self.lines.append(b"Erase failed\r\n")
        return len(command)

    def readline(self):
        self.factory.events.append("readline")
        return self.lines.pop(0) if self.lines else b""

    def close(self):
        self.closed = True
        self.factory.events.append("close")


class FakeSerialFactory:
    def __init__(
        self,
        *,
        erase_result="success",
        initialize_error=False,
        invalid_status=False,
        open_error=False,
    ):
        self.erase_result = erase_result
        self.initialize_error = initialize_error
        self.invalid_status = invalid_status
        self.open_error = open_error
        self.calls = []
        self.events = []
        self.instances = []

    def __call__(self, **kwargs):
        self.calls.append(kwargs)
        self.events.append(("open", kwargs["port"], kwargs["baudrate"]))
        if self.open_error:
            raise SerialException("port unavailable")
        instance = FakeSerial(self, kwargs)
        self.instances.append(instance)
        return instance

    @property
    def last(self):
        return self.instances[-1]


class FakeModemFactory:
    def __init__(
        self,
        serial_factory,
        *,
        recv_payload=b"",
        recv_result="default",
        send_result=True,
        interrupt_recv=False,
    ):
        self.serial_factory = serial_factory
        self.recv_payload = recv_payload
        self.recv_result = recv_result
        self.send_result = send_result
        self.interrupt_recv = interrupt_recv
        self.sent_payload = None
        self.operations = []

    def __call__(self, getc, putc):
        self.getc = getc
        self.putc = putc
        return self

    def recv(self, stream):
        self.operations.append("recv")
        self.serial_factory.events.append("recv")
        stream.write(self.recv_payload)
        if self.interrupt_recv:
            raise KeyboardInterrupt
        if self.recv_result == "default":
            return len(self.recv_payload)
        return self.recv_result

    def send(self, stream):
        self.operations.append("send")
        self.serial_factory.events.append("send")
        self.sent_payload = stream.read()
        if self.send_result:
            self.serial_factory.last.lines.extend([b"\r\n", b"Write done\r\n"])
        return self.send_result


def no_sleep_factory(events):
    def no_sleep(seconds):
        events.append(("sleep", seconds))

    return no_sleep
