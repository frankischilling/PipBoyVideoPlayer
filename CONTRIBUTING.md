# Contributing

Contributions should keep the release build working, include relevant tests, and distinguish verified engine behavior from assumptions.

## Working agreement

- Keep changes small enough to review in one sitting.
- Separate observed behavior from assumptions.
- Link primary documentation or source code for engine and library claims.
- Record decisions that change a public contract in `docs/decisions-and-open-questions.md`.
- Do not add sample media unless its redistribution terms are written down and compatible with the project license.
- Do not commit personal MP4 files, game assets, generated crash dumps, or local VNV paths.
- Do not put `agent` in a branch name.

Suggested branch prefixes are `feature/`, `fix/`, `docs/`, `test/`, and `release/`. Use a focused branch for implementation work and keep `main` releasable.

## Documentation style

Write in plain technical English. State the failure mode and the expected behavior. Avoid promises that have not been tested. Use sentence-case headings, straight quotation marks, and ordinary punctuation.

Architecture documents describe the current intended design. Changelogs and decision records are the right place to explain what changed.

## Evidence for reverse-engineered behavior

A useful engine note contains:

- the exact FalloutNV executable version;
- the xNVSE version;
- the relevant mod list or smallest reproduction profile;
- the address-discovery method or named engine object;
- the observed render or menu ordering;
- a debugger trace, log excerpt, or repeatable test;
- known failures under Alt+Tab, resolution changes, or DXVK.

Do not publish proprietary game code or large decompiled listings. Describe the interface and keep quoted material to the minimum needed for review.

## Commits

Commit messages should say what the repository contains after the commit. Keep authorship tied to the contributor's configured Git identity. Do not add automated authorship notices, generated-by footers, or similar markers.

## Pull request checklist

- The change has a focused scope and keeps `main` releasable.
- Technical claims have a source or a clearly labeled verification task.
- New dependencies have ownership, license, update, and packaging notes.
- New hooks include an uninstall or shutdown path and a conflict test.
- Test matrix changes include pass criteria instead of a bare list of environments.
- Public text has received a plain-language editorial pass.
