# Manual hardware validation

This is a manual checklist. It does not run commands automatically and does not establish in-circuit support. Follow [Hardware and wiring](HARDWARE.md), use a separate and fully isolated flash chip, and stop if the chip identity, voltage, level shifter, or wiring is uncertain.

Store all readbacks outside the repository or give them a `.rom` extension, which the repository ignores. Never add firmware dumps, board-specific BIOS images, serial logs containing private data, or validation artifacts to a release.

## A. Non-destructive validation

Complete this section before considering erase or write. A second read is useful for repeatability, but is optional.

### Preparation

- [ ] Disconnect every power source.
- [ ] Confirm that the flash is separate from, or fully isolated from, other circuitry.
- [ ] Inspect package orientation and every connection against the actual chip data sheet.
- [ ] Record or photograph the complete flash marking; do not infer it from capacity or JEDEC ID alone.
- [ ] Confirm `/WP` and `/HOLD` are held inactive at 3.3 V for standard SPI operation.
- [ ] Confirm Arduino, flash supply, and level shifter share a common GND.
- [ ] Confirm the 100 nF VCC-to-GND capacitor is close to the flash.
- [ ] With the signal wiring checked, measure the regulated flash VCC and record it below. Stop if it is outside the actual chip's permitted range.
- [ ] Confirm no flash pin can receive 5 V.

### Firmware and initialize

Upload is an explicit manual hardware action. Replace `<arduino-port>` with the actual Uno upload port:

```sh
pio run -e uno -t upload --upload-port <arduino-port>
```

- [ ] Record the exact firmware commit or working-tree identifier.
- [ ] Upload completed successfully to an Arduino Uno.
- [ ] Close PlatformIO Monitor and every other program that might hold the serial port.

The host CLI automatically waits for the Uno reset, clears stale startup input, sends initialize (`b"0"`), and requires `CHIP READY` plus the flash-size response before beginning the requested read. Record the initialize output. An unknown or unsupported JEDEC ID must not be treated as proof of a defective chip.

### Full read and evidence

Choose an ignored `.rom` destination and replace `<serial-port>` with `/dev/...` or `COM...` as appropriate:

```sh
python xmodembios.py read hardware-validation-read.rom <serial-port> 115200
```

- [ ] Command exit code is 0.
- [ ] Initialize completed and reported the expected 16 MiB capacity.
- [ ] Output is a regular file of exactly 16,777,216 bytes.
- [ ] Record the CLI-reported SHA-256 below.
- [ ] Copy the readback and its hash to safe storage outside the repository.
- [ ] If the chip's current content is known, compare the full-file SHA-256 with the known dump and record both values.
- [ ] Optionally perform a second complete read and compare its SHA-256 with the first. This is recommended for unstable or newly assembled wiring, but is not mandatory.

Do not continue to destructive validation if reads are unstable, hashes differ unexpectedly, initialization is inconsistent, or electrical measurements are uncertain.

## B. Destructive validation

> **Manual decision and irreversible risk:** Continue only when the user explicitly chooses to replace the flash contents and at least two verified backup copies of the original read exist in separate safe locations.

### Authorization and input checks

- [ ] At least two backup copies exist and both hashes match the recorded original-read hash.
- [ ] The exact target motherboard/device and compatible target ROM have been independently confirmed.
- [ ] The target ROM is a regular, readable file of exactly 16,777,216 bytes.
- [ ] The target ROM SHA-256 has been recorded.
- [ ] The electrical setup remains unchanged and stable since the successful read.
- [ ] The user has explicitly decided to perform the destructive erase/write sequence.

### Erase, write, and optional verify

Run each operation separately and record its exit code and relevant hash. Replace placeholders with the validated paths and port:

```sh
python xmodembios.py erase <serial-port> 115200
python xmodembios.py write <target.rom> <serial-port> 115200
python xmodembios.py verify <target.rom> <serial-port> 115200
```

- [ ] Erase exit code and firmware result recorded.
- [ ] Write exit code, input size, and input SHA-256 recorded.
- [ ] Optional verify was either explicitly selected and recorded, or explicitly marked not run.
- [ ] If verify ran, its reference/readback hashes, exit code, and any first differing offset were recorded.
- [ ] No automatic erase or automatic verify was assumed.
- [ ] All power was removed before the chip was moved or reconnected.

Verify is a separate, optional full readback. Skipping it must be recorded; it must not be reported as successful or automatic.

## Fillable validation record

| Field | Result |
| --- | --- |
| Date/time and timezone | |
| Operator | |
| Arduino board | |
| Firmware commit / working-tree ID | |
| Flash marking (exact text/photo reference) | |
| Observed JEDEC ID | |
| Level shifter / circuit | |
| Measured flash VCC | |
| Serial port | |
| Baud rate | 115200 |
| First read filename | |
| First read size | |
| First read SHA-256 | |
| Optional second read SHA-256 | Not run / |
| Expected/known dump SHA-256 | Unknown / |
| Read comparison result | Not applicable / Match / Mismatch |
| Backup copy locations | |
| Target ROM filename | Not applicable / |
| Target ROM SHA-256 | Not applicable / |
| Erase result and exit code | Not run / |
| Write result and exit code | Not run / |
| Verify result and exit code | Not run / Match / Mismatch / Error |
| Power removed before chip movement | |
| Notes | |

## Stop conditions

Stop, disconnect power, and investigate before continuing if any of these occurs:

- measured voltage is outside the actual chip's limits;
- the exact pinout or package orientation is uncertain;
- initialize is inconsistent or reports an unsupported identity;
- repeated reads differ without an understood reason;
- the full backup size or hash is missing;
- erase/write returns nonzero, times out, or reports a firmware error;
- the level shifter, supply, or wiring becomes unstable.
