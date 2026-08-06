---
name: documentation-maintenance
description: Maintain AirCube-TinieTronics repository documentation when implementation behavior, architecture, interfaces, commands, constraints, discoveries, paths, or documentation structure changes.
---

# Documentation Maintenance

Read this entire `SKILL.md` before applying it. Read every referenced project document needed for the task.

## Activate when

- Code, hardware, integrations, protocols, build/test commands, dependencies, or architecture change.
- A reusable discovery, decision, blocker, validation result, or corrected misconception should survive the current task.
- Documentation is added, removed, moved, renamed, split, archived, reviewed, or suspected stale.

## Procedure

1. Read `../../../docs/INDEX.md`, `../../../docs/progress_tracker.md`, and `../../../docs/AI_agent_instructions.md`.
2. Record Git state if available. This snapshot may lack `.git`; label unavailable facts rather than guessing.
3. Inspect current implementation and all affected producers/consumers before editing prose. Upstream is context, not fork authority.
4. Choose the owner document:
   - Architecture/execution/data flow: `../../../docs/architecture.md`.
   - File ownership/navigation/artifacts: `../../../docs/repository_map.md`.
   - Commands/dependencies/check results: `../../../docs/build_and_test.md`.
   - Wire formats/client mappings: `../../../docs/protocols_and_integrations.md` or `../../../docs/BLE_GATT_PROTOCOL.md`.
   - PCB/mechanical/desktop tooling: `../../../docs/hardware_and_tools.md`.
   - Verified defects/risks/unresolved questions: `../../../docs/known_issues.md`.
   - Current work and handoff: `../../../docs/progress_tracker.md`.
5. Put stable detail in the owner document. Add only a concise status/link in the tracker; do not use the tracker as permanent technical reference.
6. Update architecture and repository maps when components, execution paths, boundaries, or ownership change.
7. Update every affected cross-link and `../../../docs/INDEX.md` after documentation or skills are added, removed, moved, renamed, split, or archived.
8. Record meaningful work in reverse chronological order in the tracker, including validation and remaining work.

## Safety and quality rules

- Cite repository paths and important symbols. Separate verified behavior, inference, and unknowns.
- Never claim a command, build, test, hardware behavior, binary/CAD inspection, or client compatibility succeeded unless actually verified.
- Preserve unique reasoning, decisions, limitations, and history while removing accidental duplication.
- Keep summaries/navigation within the first 100 lines. Split around 300-400 lines, multiple independent topics, or poor scanability.
- Archive old tracker entries under `../../../docs/progress_archive/YYYY-MM.md`; never delete useful history merely to shorten it. Link every archive from the tracker and index.
- Do not expose secrets or local credentials.

## Detect and validate staleness

1. Compare documented paths/symbols/config values against current files and build source lists.
2. Search renamed terms and old constants across docs and implementation.
3. Verify relative Markdown links, anchors where practical, and all paths/commands.
4. Confirm every top-level directory and every documentation/skill file is indexed.
5. Check key content remains in each document's first 100 lines.
6. Run the narrowest safe syntax/build/test checks available and record exact outcomes; explicitly record unavailable checks.
7. Review the final status/diff when Git exists and ensure no unrelated product files changed.
