# Release checklist

This checklist prepares a release; it does not authorize hardware operations, commits, tags, pushes, or GitHub releases. Complete it for the exact candidate commit.

## Repository and artifacts

- [x] Review `git status --short` and every tracked/untracked change.
- [x] Confirm only intended project files are included.
- [x] Confirm `output.rom`, `newbios2.bin`, other `*.rom` readbacks, and `*.part` files are ignored and absent from the index.
- [x] Confirm `.venv/`, `.pio/`, `__pycache__/`, and test-generated temporary files are absent from the index.
- [x] Check tracked and intended untracked file sizes; investigate every unexpected binary or large file.
- [x] Confirm no hardware readback, target BIOS image, secret, token, private key, or machine-specific path is included.

## Automated verification

- [x] Python 3.14.7 dependency installation succeeds from `requirements.txt`.
- [x] `python -m pip check` passes.
- [x] Production Python modules pass `py_compile` or `compileall`.
- [x] All Python unit tests pass on Ubuntu and Windows.
- [x] Main CLI and all four subcommand help smoke tests pass.
- [x] All 71 firmware characterization tests pass with zero missing safeguards and zero unexpected failures.
- [x] ASan and UBSan report no error.
- [x] AVR syntax check passes without production warnings.
- [x] `pio run -e uno -t clean` and `pio run -e uno` pass.
- [x] PlatformIO uses Core 6.2.0 and Atmel AVR platform 5.3.0.
- [x] GitHub Actions jobs are green for candidate commit `d7a3b6c` ([run 35598136126](https://github.com/bsvdoom/ArduinoBIOSProgrammer/actions/runs/35598136126)).

## Documentation and hardware status

- [ ] README quick-start, CLI syntax, memory figures, and known limitations match the candidate.
- [ ] Repository-local Markdown links resolve and key external technical links are reviewed.
- [ ] `CHANGELOG.md` has an accurate Unreleased section with no premature release date or tag.
- [ ] `docs/HARDWARE_VALIDATION.md` was followed, or the release clearly states that post-modernization hardware validation remains pending.
- [ ] Hardware evidence records the exact flash marking, voltage, board, firmware commit, file size, hashes, exit codes, and whether optional verify ran.
- [ ] The exact chip model is not claimed without marking/photo evidence.
- [ ] The unresolved upstream/component license and provenance status has been reviewed; do not add or imply a project-wide license without authority.

## Version and publication

- [ ] Choose and document the release version.
- [ ] Update version-bearing project metadata consistently.
- [ ] Replace the Unreleased heading with the chosen version/date only when the release decision is final.
- [x] Review the complete candidate diff.
- [x] Create the release-candidate commits.
- [x] Push `modernization/release-candidate` through commit `d7a3b6c`.
- [ ] Create the annotated Git tag.
- [ ] Push the release tag.
- [ ] Create the GitHub release with changelog, limitations, build/test status, hardware status, and license/provenance caveat.

The `modernization/release-candidate` branch was manually pushed through `d7a3b6c`. The public GitHub Actions API verifies that its CI run completed successfully and all four jobs are green: firmware characterization on Ubuntu, Python 3.14.7 on Ubuntu and Windows, and the PlatformIO Arduino Uno build. See the [branch-filtered Actions page](https://github.com/bsvdoom/ArduinoBIOSProgrammer/actions?query=branch%3Amodernization%2Frelease-candidate).

Hardware validation remains **NOT RUN**. No Arduino upload, physical flash read/erase/write/verify, merge, pull request, tag, or GitHub release has been performed. Real Arduino Uno and flash-hardware validation is still required before a stable release.
