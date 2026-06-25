# ZenithOS Documentation Status

This file replaces three older Markdown summaries whose contents described
states that no longer match the live codebase as of Beta 2 (June 2026).

| Old file | Replaced by |
| --- | --- |
| `NEW_FEATURES_SUMMARY.md` (May 27, 2026) | this file |
| `AUTONOMOUS_WORK_SUMMARY.md` (May 27, 2026) | this file |
| `OS_BUILDING_GUIDE.md` (May 27, 2026) | this file |

## Canonical status sources

The authoritative, current state of ZenithOS lives in:

- **`OS/BUILD_STATUS.md`** — short, CI-generated, top-of-tree summary.
- **`OS/workstation/release/RELEASE_NOTES.md`** — current release highlights.
- **`OS/workstation/release/KNOWN_ISSUES.md`** — live list of accepted risks.
- **`OS/workstation/release/CHANGELOG.md`** — historical per-release changes.
- **`OS/workstation/README.md`** — workstation-track design + build shape.
- **`OS/workstation/roadmap.md`** — product roadmap.
- **`OS/workstation/architecture.md`** — current architecture.

## Why the May 27 summaries were retired

They described a pre-Beta-1 state in which:

- The seed ISO needed a sudo-only rebuild to pick up theme assets.
- Only 5 first-boot Flatpaks were configured (now 10).
- The installer had no `snapper create-config` wiring.
- No `zenith-snapshot` CLI existed.
- No Lenovo V14-ADA hardware probe or validation checklist existed.
- No CI workflow existed.

All of these have since landed. The redirected canonical docs track what
actually ships.

## Historical archive

If you need the historical content, the original files are preserved in
commit history (use `git log -- OS/NEW_FEATURES_SUMMARY.md` etc.).
