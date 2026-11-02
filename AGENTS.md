# AirCube project workflow

Read `docs/INDEX.md`, `docs/AI_agent_instructions.md`, `docs/progress_tracker.md`, and `docs/development_workflow.md` before changing this repository. Follow the relevant project skills under `.agents/skills/`.

- Treat this fork and its connected ESP32-H2 hardware as the source of truth. Inspect the Git branch, remote, and working tree before edits; preserve unrelated work.
- For every firmware-affecting improvement, update `firmware/version.txt` according to `docs/development_workflow.md`. Keep documentation and `docs/progress_tracker.md` current as part of the same change.
- For a feature, run relevant checks, a clean ESP-IDF firmware build, and inspect build warnings/errors. When the AirCube is connected, attempt to flash the validated image and check boot, serial output, and affected behavior; ask for help with physical access when needed. Never erase user data or change radio/pairing configuration as an incidental step. Document failed or unavailable checks honestly.
- Commit completed features and push to this fork's `origin` after checking the destination branch and obtaining a passing validation result. Never push to `upstream` or overwrite remote history. Do not present an untested change as a validated release.
- The current checkout may contain uncommitted documentation from prior work. Do not silently include, discard, or overwrite it in unrelated commits.
