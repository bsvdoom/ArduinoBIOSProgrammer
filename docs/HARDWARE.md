# Hardware and wiring

This guide describes the currently documented setup: an Arduino Uno connected to a **separate, unpowered 3.3 V SPI flash chip** through suitable level shifting. It does not establish support for in-circuit programming.

## Safety first

The Arduino Uno's digital logic reference is 5 V. The W25Q128FV data sheet used as a command and electrical reference specifies a 2.7-3.6 V supply range. A 3.3 V flash must never receive 5 V on VCC or an I/O pin. The 100 nF value below is a conservative local-decoupling recommendation for that supply rail, not a claim that the data sheet identifies the unknown physical chip.

Use all of the following:

- a regulated 3.3 V supply suitable for the chip;
- appropriate 5 V to 3.3 V level translation on `/CS`, CLK, DI/MOSI, and DO/MISO;
- a common ground between the Uno, level shifter, supply, and flash;
- a 100 nF ceramic decoupling capacitor directly between flash VCC and GND, physically close to the package;
- wiring performed and checked with every supply disconnected.

The historic setup used an unidentified four-transistor Chinese level-shifter module. Its exact circuit, direction behavior, and signal integrity are unknown, so it is not a specific or guaranteed hardware recommendation.

> **Data-loss warning:** Chip erase destroys the complete flash contents, and write replaces data. Obtain, hash, and safely copy a full original read before erasing.

## Wiring

The following table is for the common eight-pin SPI assignment represented by the W25Q128FV reference data sheet. Confirm the actual chip marking, package orientation, pin 1 indicator, pinout, voltage, and `/WP`/`/HOLD` behavior in the actual device data sheet.

| Flash pin | Name | Function | Connection | Arduino Uno |
| ---: | --- | --- | --- | --- |
| 1 | `/CS` | active-low chip select | through level shifter | D10 / SS |
| 2 | DO / IO1 | flash data out, SPI MISO | through level shifter | D12 / MISO |
| 3 | `/WP` / IO2 | active-low write protect in SPI mode | direct to regulated 3.3 V | - |
| 4 | GND | supply ground | common ground | GND |
| 5 | DI / IO0 | flash data in, SPI MOSI | through level shifter | D11 / MOSI |
| 6 | CLK | SPI clock | through level shifter | D13 / SCK |
| 7 | `/HOLD` / IO3 | active-low hold in SPI mode | direct to regulated 3.3 V | - |
| 8 | VCC | flash supply | regulated 3.3 V | - |

Do not use the Uno's 5 V pin for the flash. Ensure the chosen 3.3 V source and level shifter are suitable for the actual device and SPI signaling. The project does not characterize a specific level-shifter module.

## Separate chip versus in-circuit use

The documented configuration assumes the flash is removed from, or otherwise fully isolated from, the motherboard. In-circuit programming can introduce motherboard standby rails, parallel drivers, large board loads, and back-power paths. Those conditions were not tested and are not claimed as supported.

Do not connect the programmer to a motherboard merely because its main power appears off. This documentation does not define a safe in-circuit isolation procedure.

## Before applying power

- Confirm that the chip is separate from all other powered circuitry.
- Read the actual part marking and obtain its manufacturer data sheet.
- Check package orientation and every pin number with a continuity meter if needed.
- Confirm no signal or supply path can place 5 V on the flash.
- Confirm regulated 3.3 V at the disconnected supply output.
- Confirm common ground.
- Confirm `/WP` and `/HOLD` are held inactive at 3.3 V.
- Confirm D10, D11, D12, and D13 reach the intended flash pins through the level shifter.
- Fit the 100 nF VCC-to-GND capacitor close to the flash.
- Recheck for shorts before connecting USB or 3.3 V power.

## First connection

Upload the firmware only to the Arduino Uno. Connect the isolated flash only after checking the wiring. The first host-side flash operation should be a full `read`; do not start with erase or write. Preserve the 16,777,216-byte result and its SHA-256 hash before proceeding.

## Troubleshooting

### Unknown JEDEC ID or `INITIALIZE FAILED`

- Remove power and recheck pin 1 orientation, VCC, GND, `/CS`, CLK, DI, and DO.
- Confirm `/WP` and `/HOLD` are inactive high for standard SPI operation.
- Confirm the level shifter is appropriate for the signal directions and is powered correctly.
- Verify the actual part data sheet and voltage range.
- An electrically valid but unsupported chip may still be rejected. The firmware supports only its explicit identification table; do not assume that an `EF 40 18` fixture proves the physical chip model.

### Unstable reads or XMODEM failures

- Shorten wiring and keep the decoupling capacitor at the flash package.
- Check ground continuity and 3.3 V stability while active.
- Inspect the level shifter: slow transistor-based boards can distort SPI edges.
- Avoid breadboard contact problems and long flying leads.
- Close other programs using the serial port and confirm the selected port and 115200 baud.
- Repeat a non-destructive read and compare hashes; do not erase while reads are unstable.

### The serial port is missing or cannot be opened

- On Linux, list ports with `.venv/bin/python -m serial.tools.list_ports` and verify device permissions/udev rules.
- On Windows, use `python -m serial.tools.list_ports`, confirm the USB driver, and pass the shown `COM` name unchanged.
- Reopening the port resets many Uno boards; the CLI deliberately waits for startup before initialization.

## Primary references

- [Winbond W25Q128FV official documentation entry and data-sheet download](https://www.winbond.com/hq/support/documentation/?__locale=en&category=%2F.categories%2Fresources%2Fdatasheet%2F&family=%2Fproduct%2Fcode-storage-flash-memory%2Fserial-nor-flash%2Findex.html&line=%2Fproduct%2Fcode-storage-flash-memory%2Findex.html&pno=W25Q128FV)
- [W25Q128FV data sheet, direct PDF mirror used by the characterization](https://www.pjrc.com/teensy/W25Q128FV.pdf)
- [Arduino Uno R3 official documentation](https://docs.arduino.cc/hardware/uno-rev3)
- [Arduino Uno R3 official data sheet](https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf)
- [Arduino Uno official pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf)

The W25Q128FV documents are compatibility references for the implemented command set and electrical warnings; they do not identify the unrecorded physical chip.
