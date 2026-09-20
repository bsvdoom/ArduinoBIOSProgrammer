# Arduino BIOS Programmer

## Project overview

Arduino BIOS Programmer is an Arduino Uno-based SPI flash reader/writer. The current host CLI supports a complete **16 MiB (16,777,216-byte / 128-Mbit)** image and transfers it with XMODEM over a 115200-baud serial connection.

Read, write, and verify operations are slow. Their actual duration depends on the hardware, USB/serial connection, and flash behavior. This project does not claim general support for every W25Q-family device.

## Current status

- Arduino Uno is the only board currently built and used by this project.
- A BIOS chip from an ASUS Z170I Pro Gaming motherboard was successfully read and written in an earlier state of the project. The exact chip marking was not recorded, and the modernized version still needs hardware validation.
- The confirmed image size is 16,777,216 bytes, corresponding to 128 Mbit.
- A [forum report](https://rog-forum.asus.com/t5/gaming-motherboards/sku-for-the-dip8-bios-chip-on-the-z170-pro-gaming-motherboard/td-p/1068059) naming a W25Q128FVIQ concerns the different ASUS **Z170 Pro Gaming** model; it is related context, not identification evidence for the Z170I chip.
- The current automated baseline is 71 native firmware characterization tests and 56 Python tests passing, with a warning-free Arduino Uno PlatformIO production build also passing.

## Electrical warning

> **Risk of hardware damage and data loss:** erase and write operations destroy or replace flash contents. Make and preserve a verified backup before modifying a chip.

The Arduino Uno uses 5 V logic, while the reference flash is a 3.3 V device. Never connect an Uno 5 V signal or supply directly to the flash. Use suitable bidirectional or direction-appropriate **5 V to 3.3 V level shifting** on the SPI signals, a regulated 3.3 V supply, and a common ground. The four-transistor Chinese module used historically was not identified and is not a guaranteed recommendation.

Build and verify all wiring while power is disconnected. The documented setup is for a **separate, unpowered flash chip**. In-circuit programming, motherboard standby power, and back-powering have not been validated or claimed as supported. The reference data sheet defines the flash supply limits; add a 100 nF ceramic capacitor between VCC and GND, close to the chip, as local supply decoupling.

See [Hardware and wiring](docs/HARDWARE.md) before connecting anything.

## Wiring table

| Flash pin | Signal | Connection | Arduino Uno |
| ---: | --- | --- | --- |
| 1 | `/CS` | through level shifter | D10 |
| 2 | `DO` / MISO | through level shifter | D12 |
| 3 | `/WP` | direct to regulated 3.3 V | - |
| 4 | GND | common ground | GND |
| 5 | `DI` / MOSI | through level shifter | D11 |
| 6 | CLK | through level shifter | D13 |
| 7 | `/HOLD` | direct to regulated 3.3 V | - |
| 8 | VCC | regulated 3.3 V | - |

Always verify the pinout and voltage limits in the data sheet for the **actual chip and package** before applying power.

## Prerequisites

- Git.
- [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html) or the VS Code PlatformIO extension.
- Python **3.14.7**. [pyenv](https://github.com/pyenv/pyenv#readme) is recommended on Linux; native Windows users should install the exact Python version by a Windows-supported method.
- USB serial access for the current user. On Linux this may require the appropriate device permissions or PlatformIO udev rules; on Windows the Uno/USB-serial driver must be installed.
- A safe 3.3 V supply, suitable level shifting, and the disconnected flash chip.

## Quick start

These commands prepare the software and build the firmware. Review the hardware guide before upload or connection.

```sh
git clone https://github.com/bsvdoom/ArduinoBIOSProgrammer.git
cd ArduinoBIOSProgrammer
pyenv install 3.14.7
pyenv local 3.14.7
python -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
pio run -e uno
pio run -e uno -t upload
.venv/bin/python -m serial.tools.list_ports
.venv/bin/python xmodembios.py read backup.rom /dev/ttyUSB0 115200
sha256sum backup.rom
```

The committed `.python-version` already selects 3.14.7 when pyenv is configured. On Windows, create the environment with Python 3.14.7 and use `.venv\Scripts\python.exe` in place of `.venv/bin/python`. Upload only after selecting the correct Uno port and confirming the isolated-chip wiring.

The first flash operation should be a read. Record the reported SHA-256 value and store a second copy of the backup safely before any erase or write.

## CLI usage

```text
python xmodembios.py read backup.rom [serial_port] [baud_rate]
python xmodembios.py erase [serial_port] [baud_rate]
python xmodembios.py write firmware.rom [serial_port] [baud_rate]
python xmodembios.py verify firmware.rom [serial_port] [baud_rate]
```

The default port is `/dev/ttyUSB0` and the default speed is 115200 baud. A Windows port occupies the same argument position, for example:

```powershell
python xmodembios.py verify firmware.rom COM3 115200
```

Every complete image must be exactly 16,777,216 bytes. `erase`, `write`, and `verify` are separate explicit commands: **write does not erase and does not verify automatically**. Verify is optional and performs a full readback. Read and write can take a long time.

| Exit code | Meaning |
| ---: | --- |
| 0 | success |
| 1 | operation, file, protocol, or serial error |
| 2 | command-line usage error |
| 3 | verify mismatch |
| 130 | user interruption |

See the [complete CLI reference](docs/CLI.md) for file safety, timeout, `.part`, and troubleshooting details.

## Recommended safe workflow

1. Read the original chip to a backup file.
2. Record its size and SHA-256 hash; store a safe second copy.
3. Run the separate erase command only when the backup is secure.
4. Write the exact-size replacement image.
5. Optionally run the separate full-readback verify command.
6. Remove power before moving or reconnecting the chip.

Verify is strongly useful but remains an explicit, optional operation.

## Legacy scripts

`xmodembiosread.py` and `xmodembioswrite.py` remain compatibility wrappers. New users should use `xmodembios.py`.

## Development

```sh
pio run -e uno
bash tests/characterization/run_all.sh
.venv/bin/python -m unittest discover -s tests/python -p "test_*.py" -v
.venv/bin/python -m py_compile xmodembios.py xmodembiosread.py xmodembioswrite.py xmodem_transport.py xmodem_operations.py
.venv/bin/python -m pip check
```

Current results: 71 firmware characterization tests pass with ASan/UBSan enabled, 56 Python tests pass, dependency checking passes, and the Uno build uses 8,392 of 32,256 bytes of flash (26.0%) and 243 of 2,048 bytes of RAM (11.9%). No real-device upload or post-modernization hardware operation is part of these checks.

## Known limitations

- Only Arduino Uno is currently tested and built.
- The CLI accepts only exact 16 MiB complete images.
- The physical flash marking and exact model have not been verified.
- Protection bits and device-specific locks are not automatically handled or changed.
- There is no automatic retry of an entire operation.
- Write does not automatically verify.
- In-circuit programming is not validated or supported by this documentation.
- Hardware validation of the modernized firmware and CLI is still required.

## Provenance and license status

The firmware history traces to [ma5ter/ArduinoBIOSProgrammer](https://github.com/ma5ter/ArduinoBIOSProgrammer); see the local [provenance review](docs/development/PROVENANCE.md). No clear distribution license was found for the upstream firmware or all inherited components. This is a technical status statement, not legal advice.

The Python `xmodem` dependency is MIT-licensed, but that license does **not** automatically cover this repository as a whole. No project-wide `LICENSE` file is asserted here while the inherited license status remains unresolved.
