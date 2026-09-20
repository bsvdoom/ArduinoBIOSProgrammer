# Release checklist

This checklist prepares a release; it does not authorize hardware operations, commits, tags, pushes, or GitHub releases. Complete it for the exact candidate commit.

## Repository and artifacts

- [ ] Review `git status --short` and every tracked/untracked change.
- [ ] Confirm only intended project files are included.
- [ ] Confirm `output.rom`, `newbios2.bin`, other `*.rom` readbacks, and `*.part` files are ignored and absent from the index.
- [ ] Confirm `.venv/`, `.pio/`, `__pycache__/`, and test-generated temporary files are absent from the index.
- [ ] Check tracked and intended untracked file sizes; investigate every unexpected binary or large file.
- [ ] Confirm no hardware readback, target BIOS image, secret, token, private key, or machine-specific path is included.

## Automated verification

- [ ] Python 3.14.7 dependency installation succeeds from `requirements.txt`.
- [ ] `python -m pip check` passes.
- [ ] Production Python modules pass `py_compile` or `compileall`.
- [ ] All Python unit tests pass on Ubuntu and Windows.
- [ ] Main CLI and all four subcommand help smoke tests pass.
- [ ] All 71 firmware characterization tests pass with zero missing safeguards and zero unexpected failures.
- [ ] ASan and UBSan report no error.
- [ ] AVR syntax check passes without production warnings.
- [ ] `pio run -e uno -t clean` and `pio run -e uno` pass.
- [ ] PlatformIO uses Core 6.2.0 and Atmel AVR platform 5.3.0.
- [ ] GitHub Actions jobs are green for the exact candidate commit.

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
- [ ] Review the complete candidate diff.
- [ ] Create the release commit.
- [ ] Create the annotated Git tag.
- [ ] Push the commit and tag.
- [ ] Create the GitHub release with changelog, limitations, build/test status, hardware status, and license/provenance caveat.

The commit, tag, push, and GitHub release items intentionally remain unchecked in 12A/12.
