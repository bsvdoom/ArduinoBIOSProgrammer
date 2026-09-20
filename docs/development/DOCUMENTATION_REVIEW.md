# Public documentation and metadata review (11/12)

- Review date: 2026-09-21; timezone: Europe/Budapest.
- Scope: public documentation, project metadata, document layout, ignore rules, links, and executable-bit correction.
- Firmware behavior, Python business logic, CLI syntax, PlatformIO configuration, dependency versions, and ROM contents were not changed.
- No upload, serial-port access, flash operation, staging, or commit was performed.

## Documentation changes

`README.md` was rewritten in English as a standalone public entry point. It now covers the project status, electrical and data-loss warnings, named wiring, prerequisites, Linux-oriented quick start with Windows notes, exact CLI syntax and exit codes, a safe workflow, legacy wrappers, development commands, known limitations, provenance, and unresolved license status.

Two public guides were added:

- `docs/HARDWARE.md`: voltage and level-shifting requirements, pin-by-pin wiring, isolated-chip scope, pre-power checklist, troubleshooting, and primary sources;
- `docs/CLI.md`: full command and argument reference, defaults, Linux/Windows examples, exact size rule, operation separation, serial startup order, timeouts, atomic read behavior, verify comparison, interruption behavior, and serial troubleshooting.

The following existing development records were moved, not copied, to `docs/development/`:

- `AUDIT.md`
- `BASELINE.md`
- `PROVENANCE.md`
- `CHARACTERIZATION.md`
- `BUILD_FIXES.md`
- `XMODEM_RETRY_FIX.md`
- `XMODEM_WRITE_FINALIZATION_FIX.md`
- `FLASH_CHARACTERIZATION.md`
- `FLASH_TIMEOUT_WEL_FIX.md`
- `FLASH_BOUNDS_FIX.md`
- `PLATFORMIO_BUILD.md`
- `PYTHON_ENVIRONMENT.md`
- `PYTHON3_MIGRATION.md`
- `CLI_IMPLEMENTATION.md`

Historical machine-specific absolute paths were replaced with `$HOME`, `~`, `<pyenv-root>`, or `<repository>` notation. Historical descriptions were otherwise retained.

## Sources checked

Technical statements were checked against the repository source and records plus these primary or official references:

- [Winbond W25Q128FV official documentation entry](https://www.winbond.com/hq/support/documentation/?__locale=en&category=%2F.categories%2Fresources%2Fdatasheet%2F&family=%2Fproduct%2Fcode-storage-flash-memory%2Fserial-nor-flash%2Findex.html&line=%2Fproduct%2Fcode-storage-flash-memory%2Findex.html&pno=W25Q128FV) and the [direct PDF mirror used by the characterization](https://www.pjrc.com/teensy/W25Q128FV.pdf);
- [Arduino Uno R3 documentation](https://docs.arduino.cc/hardware/uno-rev3), official data sheet, and official pinout;
- [PlatformIO Arduino Uno board documentation](https://docs.platformio.org/en/latest/boards/atmelavr/uno.html);
- [Python 3.14 `venv` documentation](https://docs.python.org/3.14/library/venv.html);
- [pyenv official README](https://github.com/pyenv/pyenv#readme);
- [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html);
- [`xmodem` 0.5.0 on PyPI](https://pypi.org/project/xmodem/);
- [ma5ter/ArduinoBIOSProgrammer upstream](https://github.com/ma5ter/ArduinoBIOSProgrammer) and the local `PROVENANCE.md` audit.

The W25Q128FV sheet remains a command/electrical compatibility reference. It does not prove the exact model or marking of the physical chip previously used with the ASUS Z170I Pro Gaming board. The W25Q128FVIQ forum reference concerns the different Z170 Pro Gaming board and is labeled accordingly.

## Link review

A repository-local Markdown link scan resolved links relative to each document:

```text
LOCAL_LINKS checked=4 broken=0
```

No user-specific absolute home path remains in any Markdown file. Key external technical links were opened during the review. The README links only to the public hardware and CLI guides and to the provenance review, rather than presenting all internal development records as user documentation.

The requested obsolete-instruction search found 17 lines. Every match is in a historical development record and intentionally describes superseded interpreter syntax, include-name case, the legacy writer filename, or a former work-list item. No obsolete instruction remains in `README.md`, `docs/HARDWARE.md`, `docs/CLI.md`, or `library.properties`.

## Metadata changes

`library.properties` now contains:

```text
name=Arduino BIOS Programmer
version=0.0.2
author=ma5ter, WarMonkey, contributors
maintainer=Not specified
category=Data Storage
url=https://github.com/bsvdoom/ArduinoBIOSProgrammer
architectures=avr
includes=programmer.h
```

The sentence and paragraph now describe this project instead of unrelated WebSocket functionality. The repository URL matches `git remote -v`. The maintainer field explicitly records that no maintainer is specified instead of inventing a person or email address. No license field was added.

The `xmodembios.py` executable bit is set, matching its Python 3 shebang. Windows use remains `python xmodembios.py ...` or the explicit venv interpreter.

## ROM and ignore review

`.gitignore` now contains only the requested ROM/temp additions:

```gitignore
/output.rom
/newbios2.bin
*.part
```

`git ls-files -- output.rom newbios2.bin` returned no paths. `git status --short --ignored -- output.rom newbios2.bin` reported both as `!!`: present, untracked, ignored, and not staged. Neither file was read, changed, moved, or deleted in this step.

## Command review

All five CLI help commands returned exit code 0 and displayed the documented subcommands/arguments. `py_compile` passed for the unified CLI, both legacy wrappers, the transport module, and the shared operations module. Python 3.14.7 and pyenv 2.8.6 select the committed 3.14.7 version. `pip check` returned `No broken requirements found.`

Safe, local README development/build commands were executed. Clone, interpreter installation, venv creation, dependency reinstallation, upload, serial-port enumeration, flash read/erase/write/verify, and backup hashing were not repeated: the first group is already established by prior records, while the hardware/file-dependent group is intentionally excluded from this documentation-only review. Command names and arguments were checked against the installed tools and actual CLI help.

## Tests and build

| Check | Result |
| --- | --- |
| Python unit tests | **56 PASS**, 0 failures/errors |
| Firmware characterization | **71 PASS**: 12 XMODEM + 14 write/finalization + 45 flash |
| MISSING SAFEGUARD / unexpected failure | **0 / 0** in the running characterization suite |
| ASan / UBSan | enabled, no reported sanitizer error |
| `pip check` | PASS |
| CLI help and `py_compile` | PASS |
| PlatformIO Arduino Uno build | PASS |
| Program flash | 8,406 / 32,256 bytes (26.1%) |
| SRAM | 243 / 2,048 bytes (11.9%) |

Build versions: PlatformIO Core 6.2.0, Atmel AVR platform 5.3.0, `framework-arduino-avr` 5.4.0, and AVR GCC 7.3.0. The metadata update was accepted by the PlatformIO dependency finder as `Arduino BIOS Programmer @ 0.0.2`. Exactly the existing `programmer.cpp`, `winbondflash.cpp`, and `xmodem.cpp` production units compiled once each.

The only compiler diagnostics are the six previously documented `-Wsign-compare` warnings in `src/winbondflash.cpp` (lines 158, 181, 238, 263, 275, and 287). No new warning was introduced. `pip check` also printed the existing local pip-cache ownership warning before its successful dependency result; it does not affect the project venv.

## Deferred to 12/12 or later explicit work

- Perform controlled post-modernization hardware validation only after the actual chip, voltage, isolation, and wiring are confirmed.
- Record the physical chip marking/photograph and treat the exact model as unknown until then.
- Resolve the inherited project license/permission status before making a distribution-license claim.
- Decide separately whether to address the six sign-comparison warnings, protection-bit interpretation, automatic content checks, or CI; none was changed here.
- Recheck release-facing status after any future hardware result or explicitly authorized finalization step.

The 12/12 step was not started by this review.
