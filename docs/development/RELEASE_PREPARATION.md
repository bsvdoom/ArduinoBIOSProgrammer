# 12A/12 release preparation

Date: 2026-09-21

This record covers automated CI preparation, the mechanical production-warning cleanup, and release/hardware checklists. It does not record a hardware test, commit, tag, push, or GitHub release.

## Worktree review

The existing working tree from steps 1–11 was preserved. Untracked, non-ignored files were classified as intentional project work: the PlatformIO configuration, Python environment marker, Python CLI modules and tests, firmware characterization harness, development reports, public documentation, and the new release/CI files. Ignored local files included `.venv/`, `.pio/`, Python caches, `output.rom`, and `newbios2.bin`. No unexpected build product or unexplained untracked file was found, so nothing was deleted.

Neither `output.rom` nor `newbios2.bin` is tracked or staged. The ignore rules cover those names, all `*.rom` hardware readbacks, `*.part`, `.venv/`, `.pio/`, `__pycache__/`, and `*.py[cod]`.

## Baseline before warning cleanup

- Python 3.14.7: 56/56 unit tests passed.
- Production Python modules: `py_compile` passed.
- Python dependencies: `pip check` reported no broken requirements.
- Firmware characterization: 71/71 passed (12 XMODEM, 14 write/finalization, and 45 flash cases), with 0 missing safeguards and 0 unexpected failures.
- ASan/UBSan: no diagnostic.
- PlatformIO Core 6.2.0 clean Arduino Uno build: passed.
- Baseline Uno memory: 8,406/32,256 bytes flash (26.1%) and 243/2,048 bytes RAM (11.9%).
- Production compiler output: six `-Wsign-compare` diagnostics in `src/winbondflash.cpp`.

## Warning cleanup

All six diagnostics came from loop counters declared as signed `int` and compared with the unsigned result of `sizeof(array) / sizeof(array[0])`. Each loop only indexes the `partDescription` table. The six counters were changed mechanically to `size_t`.

The change does not alter the public API, array sizes, loop bounds, PROGMEM reads, JEDEC matching, address calculations, or SPI command ordering. No cast, diagnostic suppression, compiler flag, or adjacent refactor was added.

After the change, the native characterization suite, sanitizer run, AVR syntax check, and clean PlatformIO build passed without production warnings. The generated Uno program occupies 8,392/32,256 bytes flash (26.0%) and 243/2,048 bytes RAM (11.9%). The 14-byte flash difference is compiler code generation from the corrected AVR-sized index type; the six loops still perform the same number of iterations.

## Continuous integration

`.github/workflows/ci.yml` has `push`, `pull_request`, and manual `workflow_dispatch` triggers, with repository permissions limited to `contents: read`. It has no secret, serial-port, upload, monitor, hardware, local pyenv, local virtual-environment, or machine-specific path dependency.

The workflow contains three jobs:

1. **Python** — a two-entry `ubuntu-latest` / `windows-latest` matrix using exact Python 3.14.7. It installs `requirements.txt`, runs `pip check`, compiles all five production Python modules, runs all unit tests, and smoke-tests the main and four subcommand help pages.
2. **Firmware characterization** — Ubuntu runs `tests/characterization/run_all.sh`; its ASan/UBSan-enabled binaries and all 71 cases must succeed.
3. **PlatformIO Arduino Uno** — Ubuntu uses Python 3.14.7, installs exactly PlatformIO Core 6.2.0, and builds `uno`. `platformio.ini` pins Atmel AVR 5.3.0. The build does not upload or monitor. An output check fails on a production-source warning without applying `-Werror` to Arduino framework code.

Official upstream documentation/repositories were used to select the supported stable action majors: [`actions/checkout@v7`](https://github.com/actions/checkout) and [`actions/setup-python@v7`](https://github.com/actions/setup-python). The workflow structure also follows the [official PlatformIO GitHub Actions guidance](https://docs.platformio.org/en/latest/integration/ci/github-actions.html).

The YAML parses locally and its trigger, job, and matrix structure was inspected. Neither `actionlint` nor `act` is installed locally. The workflow has not been pushed and no remote GitHub Actions run has occurred; remote Ubuntu/Windows success therefore remains to be confirmed after 12C.

## Release and hardware documents

- `CHANGELOG.md` contains an undated Unreleased summary and known limitations.
- `docs/HARDWARE_VALIDATION.md` separates non-destructive read validation from explicitly authorized destructive erase/write validation and includes a fillable evidence table.
- `docs/RELEASE_CHECKLIST.md` covers repository, test, documentation, hardware, provenance, version, commit, tag, push, and release gates. Publication steps remain unchecked.

No post-modernization hardware validation was performed. The exact chip marking is still unverified. A manual validation using the actual flash data sheet and safe 3.3 V wiring is the scope of 12B.

## Large-file, ignore, and sensitive-data checks

The ignored local ROM files are each 16,777,216 bytes and are not tracked or staged. No ROM dump, build output, virtual-environment file, Python cache, partial download, or hardware readback appears among release-intended project files. The largest intended project file found was a text development report below 100 KiB; no unexplained large binary was found.

The requested sensitive-data scan found only intentional CLI example/default ports in production help text and fake tests, plus the word `token` in a checklist. It found no private-key marker, credential value, API key, or embedded machine-specific absolute path. Documented `/dev/ttyUSB…` and `COM…` examples are not credentials.

## Final local verification

- Python 3.14.7: 56/56 unit tests passed; production `py_compile`, `pip check`, and the main/read/erase/write/verify help smoke tests passed. The only pip output beyond success was a local cache-ownership warning, not a dependency error.
- Firmware: 71/71 characterization tests passed, with 0 missing safeguards and 0 unexpected failures. ASan and UBSan reported no error.
- AVR: GCC 7.3.0 `-Wall -Wextra -fsyntax-only` passed with 0 diagnostics.
- PlatformIO: Core 6.2.0 clean and rebuild passed with Atmel AVR 5.3.0, Arduino AVR framework 5.4.0, and AVR GCC 7.3.0. The CI production-warning filter was exercised locally and accepted the warning-free build.
- Uno memory: 8,392/32,256 bytes flash (26.0%) and 243/2,048 bytes RAM (11.9%).
- Harness invocation: `tests/characterization/run_all.sh` is executable, so the requested direct command and documented shell invocation both work.
- Structure: local workflow YAML parsing, trigger/job/matrix assertions, and five repository-local Markdown links passed; 0 broken local links were found.
- Repository hygiene: ignore, index, intended-file size, sensitive-data, and Git diff checks passed. The exact final worktree inventory is reported in the 12A handoff.

These checks must be repeated by the release checklist for the eventual candidate commit. They do not substitute for a remote GitHub Actions run or a physical hardware test.

## Remaining steps

- **12B:** perform and record the manual hardware validation only after the user accepts the electrical and destructive-operation risks. It has not been started here.
- **12C:** review the final candidate, run remote CI, resolve the license/provenance decision, choose a version, and only then commit, tag, push, and create a release. None of those publication actions was started here.
