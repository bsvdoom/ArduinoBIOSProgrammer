# XMODEM BIOS CLI reference

The unified Python 3 CLI reads, erases, writes, or verifies one complete 16 MiB flash image through the Arduino firmware. It does not transform payload byte order.

## Requirements

- Python 3.14.7 and the project virtual environment;
- `xmodem==0.5.0` and `pyserial==3.5` from `requirements.txt`;
- uploaded Arduino Uno firmware;
- a safely connected, isolated flash chip as described in [Hardware and wiring](HARDWARE.md).

Create the environment on Linux:

```sh
pyenv install 3.14.7
pyenv local 3.14.7
python -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

On Windows, create `.venv` with a native Python 3.14.7 interpreter and use `.venv\Scripts\python.exe`.

## Command syntax

```text
python xmodembios.py read file.rom [serial_port] [baud_rate]
python xmodembios.py erase [serial_port] [baud_rate]
python xmodembios.py write file.rom [serial_port] [baud_rate]
python xmodembios.py verify file.rom [serial_port] [baud_rate]
```

| Argument | Meaning | Default |
| --- | --- | --- |
| `file.rom` | destination for read; input/reference for write or verify | required where shown |
| `serial_port` | pySerial device name, passed unchanged | `/dev/ttyUSB0` |
| `baud_rate` | positive integer serial rate | `115200` |

The production ROM size is exactly **16,777,216 bytes**. Read accepts only a transfer of that final size; write and verify validate the input before opening the serial port.

## Examples

Linux:

```sh
.venv/bin/python xmodembios.py read backup.rom
.venv/bin/python xmodembios.py erase /dev/ttyUSB1 115200
.venv/bin/python xmodembios.py write firmware.rom /dev/ttyUSB1 115200
.venv/bin/python xmodembios.py verify firmware.rom /dev/ttyUSB1 115200
```

Windows PowerShell:

```powershell
.\.venv\Scripts\python.exe xmodembios.py read backup.rom COM3 115200
.\.venv\Scripts\python.exe xmodembios.py erase COM3 115200
.\.venv\Scripts\python.exe xmodembios.py write firmware.rom COM3 115200
.\.venv\Scripts\python.exe xmodembios.py verify firmware.rom COM3 115200
```

List candidate ports without accessing the flash:

```sh
.venv/bin/python -m serial.tools.list_ports
```

On Windows, the equivalent command can be run with `.venv\Scripts\python.exe`.

## Operations are deliberately separate

### Read

`read` initializes the firmware, sends only the read command, receives the complete chip through XMODEM, validates the exact size, and prints the size and SHA-256 hash. It does not erase or write.

Data is first written to `<destination>.part` in the destination directory. After a successful transfer, the CLI flushes and `fsync`s it, checks the exact size, and atomically replaces the final destination with `os.replace`. A failed, short, long, or interrupted transfer removes the `.part` file and preserves any pre-existing destination.

### Erase

`erase` initializes the firmware and sends only the full-chip erase command. It waits up to 300 seconds for the firmware's success or error response. It does not start write or verify. This is destructive.

### Write

`write` checks that its input is a readable regular file of exactly 16,777,216 bytes before opening the port. It prints the input size and SHA-256, initializes the firmware, sends only the write command, and requires both a successful XMODEM send and the firmware's `Write done` response.

Write does **not** erase first and does **not** verify afterward.

### Verify

`verify` validates the reference file before opening the port, initializes the firmware, and uses the read command to obtain a full readback. It never sends erase or write. The comparison runs in 64 KiB blocks, so the complete 16 MiB image is not held in memory.

The CLI prints reference and readback SHA-256 hashes. On mismatch it reports the first differing offset and both byte values; a short or long readback is also a mismatch. Its temporary readback file is removed after success, mismatch, failure, or interruption.

## Serial initialization and firmware commands

The port is opened as 115200 baud by default, 8 data bits, no parity, one stop bit, and with software and hardware flow control disabled. Opening an Uno serial port commonly resets the board, so the CLI follows this order:

1. open and configure the port;
2. wait 2 seconds for reset/startup;
3. discard stale startup input once;
4. send initialize `b"0"`;
5. wait for the documented finite firmware response;
6. send exactly one operation command: read `b"r"`, erase `b"e"`, or write `b"w"`.

The input buffer is not cleared after initialize is sent. Firmware status text is strictly decoded, known error responses are rejected, and every status wait has a finite deadline. The serial port is closed on success, expected error, and interruption.

## Timeouts

| Phase | Host-side timeout |
| --- | ---: |
| startup/reset delay | 2 s |
| command write | 2 s |
| initialization response | 10 s |
| read/write XMODEM prompt | 5 s |
| write completion response | 10 s |
| chip erase response | 300 s |

The erase deadline exceeds the firmware driver's 250-second chip-erase limit. There is no automatic retry of the complete operation.

## Exit codes

| Code | Meaning |
| ---: | --- |
| 0 | successful operation or matching verify |
| 1 | operation, file, protocol, firmware, or serial error |
| 2 | command-line usage error from `argparse` |
| 3 | verify mismatch, including length mismatch |
| 130 | interrupted by the user |

Normal status is written to stdout and errors to stderr. Expected failures do not print a traceback or binary data.

## Recommended workflow

```sh
.venv/bin/python xmodembios.py read original.rom /dev/ttyUSB0 115200
sha256sum original.rom
# Copy original.rom and its hash to safe storage before continuing.
.venv/bin/python xmodembios.py erase /dev/ttyUSB0 115200
.venv/bin/python xmodembios.py write firmware.rom /dev/ttyUSB0 115200
.venv/bin/python xmodembios.py verify firmware.rom /dev/ttyUSB0 115200
```

Verify is shown as a recommended explicit final step, not an automatic or mandatory one. Remove all power before moving the chip.

## Interruption and recovery

`Ctrl+C` returns exit code 130. During read, the incomplete `.part` file is removed and an existing final file is retained. During erase or write, host interruption cannot roll back changes already made to the flash. Re-establish a stable connection and assess the device state before issuing another destructive command.

## Serial troubleshooting

- Confirm the port with `python -m serial.tools.list_ports`; pass names such as `/dev/ttyUSB1` or `COM3` unchanged.
- Close serial monitors and other programs holding the port.
- On Linux, verify the current user has device permission and install the applicable PlatformIO udev rules if needed.
- On Windows, verify the board or USB-serial driver in Device Manager.
- A port-open reset is expected; the CLI handles it automatically and never asks for Enter.
- `INITIALIZE FAILED` may indicate wiring, voltage, level-shifting, unsupported identification, or chip-state problems. Disconnect power before inspecting wiring.
- If XMODEM transfers are unstable, do not erase. Check wiring length, decoupling, 3.3 V stability, and the level shifter, then repeat non-destructive reads.

## References

- [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)
- [xmodem 0.5.0 on PyPI](https://pypi.org/project/xmodem/)
- [Python 3.14 `venv` documentation](https://docs.python.org/3.14/library/venv.html)
- [pyenv official README](https://github.com/pyenv/pyenv#readme)
