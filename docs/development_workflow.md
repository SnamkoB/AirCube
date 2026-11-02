# Development, versioning, and release workflow

## Version policy

`firmware/version.txt` is the firmware version displayed by ESP-IDF and exposed by Zigbee Basic-cluster SW Build ID on endpoint 10. It is **not** a Git commit identifier. Start from the current `2.0.1` and use `MAJOR.MINOR.PATCH`:

- **PATCH** (`2.0.1` → `2.0.2`): compatible firmware bug fix, hardware-specific improvement, tuning, or internal change that affects the produced firmware image.
- **MINOR** (`2.0.1` → `2.1.0`): backward-compatible user-facing firmware feature. Reset PATCH to zero.
- **MAJOR** (`2.x.y` → `3.0.0`): breaking protocol, stored-data, pairing, or migration change. Plan migration and get user approval before releasing.
- Bump once per independently released firmware change, before building/flashing and in the same commit as the implementation and docs. Never reuse a version for a different released firmware image. Docs-only and integration-only changes do not need a firmware-version bump unless the firmware image changes; explain the decision in the progress tracker.

Home Assistant's SW Build ID establishes the declared version, not binary equivalence. Record the commit hash and built artifact SHA-256 in the progress tracker for a traceable release.

## Feature delivery checklist

1. Record branch, commit, remotes, and dirty files. Inspect the affected implementation, hardware constraints, integrations, known issues, and project skills. Define expected behavior and compatibility/migration needs before editing.
2. Implement the smallest coherent change with suitable tests. Update `firmware/version.txt` when firmware changes. Update owning docs, user-facing behavior/commands, known issues where appropriate, and `docs/progress_tracker.md` with decisions and remaining risks.
3. Run checks for every affected language/client. Follow `.agents/skills/firmware-build-validation/SKILL.md` for a clean ESP32-H2 build with ESP-IDF v6.0.2 and pinned Zigbee dependencies, log review, merged image, hash, and partition headroom. Stop on build failure or unexplained project warnings.
4. If the AirCube is connected and it is safe to interrupt, confirm the correct chip/port and partition layout. Flash only the validated bootloader, partition table, and factory app; **do not** use `erase-flash` or overwrite NVS/Zigbee storage. Read back the app descriptor or verify flash hashes. Capture boot and serial CLI output; test the affected sensor/LED/serial/Zigbee behavior where possible. Ask the user to check Home Assistant for Zigbee functionality that cannot be observed locally.
5. Log exact commands/results, device identity, version, artifact SHA-256, observed behavior, and any limitations in the tracker. A failed/unavailable hardware step is not a passing test: report it and arrange follow-up instead of claiming validation.
6. Review the diff and Git status; commit a completed feature with its version and docs, then push to `origin` on the intended branch when the checks pass. Never push to `upstream` or force-push. If validation is blocked or a risky step fails, coordinate with the user before publishing a release; don't conceal the limitation.

For non-firmware changes, run their specific checks and the firmware build required by the build-validation skill, but do not flash identical firmware merely to satisfy a checklist. Keep commits narrowly scoped; do not absorb unrelated local changes.
