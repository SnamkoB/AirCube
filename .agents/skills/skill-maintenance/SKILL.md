---
name: skill-maintenance
description: Create, update, consolidate, or validate AirCube-TinieTronics project skills when repeated task-specific procedures or safety constraints need discoverable agent guidance.
---

# Skill Maintenance

Read this entire `SKILL.md` before applying it. Read all referenced resources for every activated skill.

## Activate when

- A repeated project workflow needs reliable activation criteria, ordered steps, or safety rules.
- Repository behavior, paths, tools, or constraints make an existing skill stale.
- Skills overlap, conflict, fail discovery, have invalid frontmatter, or have grown difficult to scan.

Do not create a skill for one-off facts, generic engineering advice, or knowledge better owned by project documentation. Update an existing skill when its purpose already covers the workflow.

## Procedure

1. Read `../../../docs/INDEX.md`, `../../../docs/AI_agent_instructions.md`, and `../../../docs/progress_tracker.md`.
2. Inventory `../*/SKILL.md` and search descriptions/triggers for overlap.
3. Inspect relevant repository implementation and docs so the procedure reflects this fork, not upstream assumptions.
4. Decide whether to update, merge, or create. Prefer one focused skill over duplicated or conflicting skills.
5. For a new skill, create `../<name>/SKILL.md`; the directory and frontmatter `name` must match exactly.
6. Use YAML frontmatter with at least `name` and `description`. The description must state both what the skill does and when it activates, using specific searchable terms.
7. Put purpose, triggers, ordered procedure, required reading, and safety rules within the first 100 lines. State that the activated `SKILL.md` must be read completely.
8. Move lengthy examples, domain tables, scripts, or deep reference material into `references/`, `scripts/`, or another clearly linked resource only when useful.
9. Update `../../../docs/INDEX.md` and related documentation whenever skills are added, removed, moved, renamed, consolidated, or materially changed. Record meaningful work in the tracker.

## Validation and safety

- Parse/check frontmatter syntax; ensure one opening and closing `---` block.
- Confirm `name` equals the parent directory and description includes purpose plus activation condition.
- Check referenced resources and relative links exist.
- Inspect the first 100 lines for actionable essentials and the complete file for contradictions.
- Compare all skills to prevent duplicate triggers, incompatible procedures, and conflicting source-of-truth rules.
- Do not claim compatibility with an agent client unless that client was actually verified.
- Do not hide critical safety instructions only in a reference file.
- Preserve useful unique guidance when consolidating; remove duplicated prose, not knowledge.
- Record exact validation performed and anything that could not be verified.
