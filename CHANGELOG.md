# Changelog

All notable project changes will be documented in this file.

## Unreleased

### Added

- Unified Python 3 CLI with explicit `read`, `erase`, `write`, and optional `verify` commands.
- Reproducible PlatformIO Arduino Uno build with pinned Atmel AVR platform.
- Python 3.14.7 project environment and pinned runtime dependencies.
- Native, hardware-free XMODEM, Programmer, SPI flash, boundary, timeout, and error-propagation tests.
- Public hardware, CLI, provenance, development, and release-preparation documentation.
- GitHub Actions CI for Python on Ubuntu and Windows, native firmware characterization, and the Arduino Uno PlatformIO build.

### Fixed

- XMODEM retransmission buffer access after NAK.
- Exact-size XMODEM write completion, ACK/EOT ordering, and short/oversized transfer handling.
- Finite, operation-specific flash BUSY timeouts with `millis()` rollover-safe measurement.
- Write Enable Latch checking and write/erase error propagation to the Programmer layer.
- Flash read, page-program, sector, and block address/range/alignment validation.
- Custom chip-select initialization before the first SPI command.
- Six mechanical signed/unsigned table-loop warnings without changing the flash API or command sequence.

### Changed

- Legacy Python scripts now act as compatibility wrappers around shared Python 3 transport and operation logic.
- Read output uses an atomic `.part` plus `fsync` and `os.replace` workflow.
- Write and erase remain separate; write does not automatically erase or verify.

### Known limitations

- Only Arduino Uno is currently built and tested by automation.
- The CLI accepts exactly 16,777,216-byte images.
- The physical flash marking and exact chip model have not been verified.
- Modernized firmware and CLI behavior have not yet been validated on hardware.
- Protection bits and device-specific locks are not automatically changed or fully diagnosed.
- There is no automatic retry of a complete operation, automatic blank-check, or automatic write verification.
- In-circuit programming is not validated or documented as supported.
- The inherited firmware and component license/provenance status remains unresolved; no project-wide release license is asserted.
