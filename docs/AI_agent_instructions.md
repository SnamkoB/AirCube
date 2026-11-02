# AI Agent Instructions

## Required startup checklist

1. Read `docs/INDEX.md` first.
   Follow `AGENTS.md` and `docs/development_workflow.md` for the current versioning, hardware-validation, and GitHub delivery policy.
2. Read `docs/progress_tracker.md` before beginning work.
3. Record current branch, commit, remotes, and working-tree state; run `git status --short --branch`, `git rev-parse HEAD`, `git remote -v`, and `git ls-files` when Git metadata exists.
4. Inventory relevant tracked files and read existing documentation/instructions before editing. Inspect `AGENTS.md`, `CLAUDE.md`, `.agents/`, `.claude/`, `.github/`, and `docs/` when present.
5. Identify all skills relevant to the task. Read the complete `SKILL.md` for every relevant skill before following it. Read referenced skill resources when directed by that skill.
6. Locate behavior in current source using `docs/repository_map.md`; do not infer fork behavior from upstream.
7. Update `docs/progress_tracker.md` during and after meaningful work.

## Source-of-truth rules

- The checked-out files, current branch, and current implementation are authoritative. Upstream `StuckAtPrototype/AirCube` is supporting context only.
- Git metadata was restored after the 2026-08-05 snapshot. Inspect it in each session rather than relying on historical branch/commit information or inventing a verification commit.
- Verify important claims against implementation and cite paths/symbols. Label inference and unknowns explicitly.
- Never record secrets, tokens, private keys, credentials, or sensitive local configuration.
- Do not claim commands, builds, tests, hardware behavior, binary/CAD inspection, or client compatibility were verified unless actually verified.

## Finding relevant code

- Start with `docs/INDEX.md` and `docs/repository_map.md`, then search symbols rather than relying only on filenames.
- Firmware begins at `firmware/main/main.c:app_main`. Build membership is defined by `firmware/main/CMakeLists.txt`; a source file's presence does not mean it is compiled.
- Protocol contracts cross firmware and clients. Compare `serial_protocol.c`, `ble_gatt.c`, `zigbee.c`, `history.h`, desktop parsers, and all affected integrations.
- Hardware claims require checking both firmware and `kicad/`; current KiCad sources do not include the Pro SCD41/VCNL4040 population.
- Catalogue opaque CAD, images, archives, and generated manufacturing files, but do not imply deep inspection without suitable tools.

## Documentation obligations

- Update documentation whenever implementation behavior, architecture, interfaces, commands, build steps, constraints, known issues, or important understanding changes.
- Add focused documentation when existing documents are not an appropriate home for important reusable knowledge.
- Update `docs/INDEX.md` after adding, removing, moving, renaming, splitting, or archiving documentation or skills.
- Preserve valuable insights for the next agent. Move stable knowledge from the tracker into its owning technical document and link it.
- Avoid recording speculation as fact. Preserve uncertainty and validation status.
- Keep documents focused, put key navigation in the first 100 lines, and split documents that become difficult to navigate (usually around 300-400 lines or multiple independent topics).
- Remove accidental duplication without deleting unique rationale, decisions, caveats, or historical knowledge.

## Progress and validation

- Update the tracker at the start and completion of meaningful work. Include objective, files, work, validation, discoveries, rationale, remaining work, and docs/skills changed.
- Run the narrowest safe checks first. Record exact commands and outcomes; record why unavailable checks were not run.
- For firmware changes, build with the intended ESP-IDF/dependency versions and test on ESP32-H2 hardware when behavior is hardware-dependent.
- For protocol changes, validate every producer/consumer and preserve backward compatibility only where a concrete shipped contract requires it.
- For integrations, use platform validators or representative fixtures where available; do not equate syntax success with runtime compatibility.
- Before finishing, check paths, relative Markdown links, skill frontmatter/name consistency, index coverage, first-100-lines requirements, and the diff/status. Never overwrite unrelated work.

## Project-specific cautions

- BLE and Zigbee are mutually exclusive per boot; avoid documenting simultaneous operation.
- `ble_bthome.c` and `environmental.c` are present but absent from the component source list.
- Existing tests and CI are absent. Static review is not a substitute for firmware compilation or hardware validation.
- Consult `docs/known_issues.md` before modifying history, serial parsing, model detection, auto-dim, BLE history, Zigbee reports, desktop timestamps, or integration entities.
