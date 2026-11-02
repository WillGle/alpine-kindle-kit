---
status: historical
date: 2026-11-02
superseded_by: ../../status/CURRENT.md
---

# Diátaxis documentation migration plan — 2026-11-02

> **Completed 2026-11-02.** The migration was executed. The current tree is
> `tutorials/`, `how-to/`, `reference/`, `explanation/`, `status/`, `evidence/`,
> `history/` plus `docs/README.md`. `docs/status/STATUS.json` is the machine
> source, `CURRENT.md` and `EVIDENCE-INDEX.md` are generated, and
> `tools/docs/check.py` plus Markdown lint run in `make check` and docs CI.
> This document remains as the historical plan of record; see
> [`../../status/CURRENT.md`](../../status/CURRENT.md) for current facts.

## Completion record

- [x] Phase 0 — inventory and current/historical decisions recorded.
- [x] Phase 1 — `STATUS.json`, generated `CURRENT.md`, evidence index and
      `tools/docs/check.py`.
- [x] Phase 2 — current docs moved into Diátaxis categories with front matter
      and links verified.
- [x] Phase 3 — completed plans, old technical material and superseded evidence
      moved under `history/` with banners.
- [x] Phase 4 — backlog split; B13–B24 table break fixed; evidence index and
      provenance added; host paths removed from current evidence.
- [x] Phase 5 — current paint, screen, suspend and image-headroom claims updated.
- [x] Phase 6 — docs checker, Markdown lint, CI step and pre-push gate added;
      rollback tag `docs-pre-diataxis-20261102` created.

Final verification (2026-11-02):

- `nix-shell tools/build/nix-shell.nix --run "make check CC=cc"` → **CHECK PASSED**.
- Fresh detached worktree at the pre-migration commit with the migrated docs and
  `tools/docs/check.py` applied → **DOCS CHECK PASSED**.
- Same fresh checkout with `npx --yes markdownlint-cli2@0.18.1` → **0 errors**
  across 46 current Markdown sources.
- The completed plan now lives under `docs/history/` and is marked historical.

- Status: **completed 2026-11-02 (historical execution record).**
- Scope: documentation structure, status/provenance, links, and docs CI.
  No runtime/source behavior changes.
- Trigger: document-phase audit on 2026-11-02. The docs are evidence-rich but
  have multiple “current” sources, stale paint/build claims, broken links,
  unindexed evidence, and historical plans mixed into current technical trees.
- Out of scope: rewriting technical conclusions, deleting historical evidence,
  changing device state, or adding a full documentation website.
- Rule: one category move per PR. Use `git mv`, not copy. Do not rewrite
  historical content; add a status banner/front matter and move it to history.
- Backlog: tracked as **B25**.

## 1. Goal

Adopt the Diátaxis model as the documentation contract:

```text
tutorials/       learning-oriented: guided first success
how-to/          task-oriented: build, install, operate, recover
reference/       information-oriented: contracts, paths, commands, limits
explanation/     understanding-oriented: architecture, boot flow, display theory
status/          current truth: status, backlog, evidence index
history/         append-only: completed plans, superseded docs, old evidence
```

The migration is successful when:

1. Every maintained doc lives in exactly one category or `history/`.
2. Every current doc declares its status and verified build/date.
3. `CURRENT` and the evidence index are generated or checked, not hand-maintained.
4. There are zero broken local links in current docs.
5. Historical docs are reachable but cannot be mistaken for current instructions.
6. CI enforces structure, links, front matter, and forbidden local paths.

## 2. Principles

- **One current source:** `docs/status/STATUS.json` is the machine source;
  `docs/status/CURRENT.md` is generated from it or checked against it.
- **Current vs historical is explicit:** front matter, not prose at the bottom.
- **History is append-only:** never rewrite old measurements or delete evidence;
  mark it `historical`/`superseded` and link forward.
- **No duplication:** move with `git mv`; do not keep a “new” copy beside the old.
- **Links are code:** run a local-link checker in CI.
- **No framework first:** plain Markdown and stable indexes are enough. A site
  generator (MkDocs Material, mdBook, Docusaurus) can come later if needed.

## 3. Target tree

```text
docs/
  README.md                     # entry point; what is current
  status/
    STATUS.json                 # single machine-readable status
    CURRENT.md                  # generated/checked operator status
    BACKLOG.md                  # open items only; history stays separate
    EVIDENCE-INDEX.md           # generated index of current evidence
  tutorials/
    01-build-private-candidate.md
    02-first-install-community.md
    03-first-boot.md
  how-to/
    build-arm-binaries.md
    install-update.md
    operate-reader.md
    recover-usb.md
    qemu-gate.md
    capture-evidence.md
    package-source.md
  reference/
    repository-layout.md
    workspace.md
    ipc.md
    app-package.md
    device-profile-pw3.md
    paint-profile-engine.md
    display-programming-rules.md
    cli.md
  explanation/
    architecture.md
    boot-and-recovery.md
    eink-display.md
    prior-art.md
    decisions/
  evidence/
    README.md                   # current evidence index
    <current evidence dirs/files>
  history/
    README.md                   # old-path map + history index
    backlog-history.md
    handoff-2026-10-31.md
    plans/
    specs/
    technical/
    evidence/
```

`docs/BACKLOG.md` can remain as a compatibility entry pointing to
`docs/status/BACKLOG.md` or be moved outright in the final cleanup. The
important rule is: **open work lives in `status/`; history lives in `history/`.**

## 4. Migration mapping

| Current path | Target | Notes |
| --- | --- | --- |
| `docs/README.md` | `docs/README.md` | Rewrite as a short entry: status, tutorials, how-to, reference, explanation, history |
| `docs/BACKLOG.md` | `docs/status/BACKLOG.md` | Keep B-IDs; move imported history to `history/backlog-history.md` |
| `docs/HANDOFF.md` | `docs/history/handoff-2026-10-31.md` | Already marked stale; add front matter and banner |
| `docs/guides/CURRENT.vi.md` | `docs/status/CURRENT.md` | Generate/check from `STATUS.json`; do not hand-edit |
| `docs/guides/BUILD-INSTALL.vi.md` | `docs/how-to/build-and-install.md` | Split build vs install if it stays too long |
| `docs/community/README.md` | `docs/tutorials/02-first-install-community.md` | English community flow; keep explicit hardware limits |
| `docs/community/BUILD.md` | `docs/how-to/build-private-candidate.md` | Build inputs/toolchain |
| `docs/community/CONTRIBUTING.md` | `docs/reference/contributing.md` or `docs/how-to/contribute.md` | Task-oriented if it has commands |
| `docs/community/LICENSES.md` | `docs/reference/licenses.md` | Reference |
| `docs/community/STATUS.json` | `docs/status/community-status.json` | One status schema with STATUS.json |
| `docs/guides/OPERATIONS.vi.md` | `docs/how-to/operate-reader.md` | Remove stale suspend/old-app-flow text |
| `docs/guides/QEMU-LOCAL.vi.md` | `docs/how-to/qemu-gate.md` | Current gate instructions |
| `docs/guides/STRUCTURE.md` | `docs/reference/repository-layout.md` | Reference |
| `docs/guides/WORKSPACE.md` | `docs/reference/workspace.md` | Reference |
| `docs/technical/PAINT-PROFILE-ENGINE.md` | `docs/reference/paint-profile-engine.md` | Canonical current paint doc |
| `docs/technical/PW3-DISPLAY-PROGRAMMING-RULES.md` | `docs/reference/display-programming-rules.md` | Mandatory reference |
| `docs/technical/PW3-EINK-PERF.md` | `docs/explanation/eink-display.md` | Explanation + measurements |
| `docs/technical/PW3-PAINT-POLICY.md` | `docs/history/technical/pw3-paint-policy-2026-10-31.md` | Old policy model; mark superseded |
| `docs/technical/EINK-UI-RENDERING.vi.md` | `docs/explanation/eink-display.md` or history | Split current explanation from old measurements |
| `docs/technical/LATENCY-AUDIT-2026-11-01.md` | `docs/explanation/eink-display.md` or evidence | Keep measurements with build hash |
| `docs/technical/APP-AUDIT.md` | `docs/reference/app-package.md` or history | Depends on current relevance |
| `docs/technical/NOTIFICATIONS.md` | `docs/reference/notifications.md` or history | Verify current applicability first |
| `docs/technical/PW3-UI-POLISH-PLAN.md` | `docs/history/plans/` | Completed plan |
| `docs/technical/PW3-UI-REFRESH-UX-PLAN-2026-11-01.md` | `docs/history/plans/` | Completed/superseded plan |
| `docs/technical/superpowers/plans/*` | `docs/history/plans/` | Completed plans; active plans stay in `status/` |
| `docs/technical/superpowers/specs/*` | `docs/reference/` or `docs/history/specs/` | Current specs vs superseded |
| `docs/technical/linux/*` | `docs/history/technical/linux/` | Investigation history; not current instructions |
| `docs/technical/investigation/*` | `docs/history/technical/investigation/` | Historical evidence/notes |
| `docs/ref/EINK-LINUX-PRIOR-ART.vi.md` | `docs/explanation/prior-art.md` | Explanation |
| `docs/evidence/*` current | `docs/evidence/` | Only current evidence here |
| `docs/evidence/*` superseded | `docs/history/evidence/` | Mark `superseded_by` |
| `docs/history/BACKLOG.md` | `docs/history/backlog-history.md` | Keep anchors/imported content |

## 5. Front matter contract

Every maintained doc outside `history/` gets front matter:

```yaml
---
diataxis: tutorial | how-to | reference | explanation
status: current | draft | deprecated
updated: 2026-11-02
verified_build: <sha256 or "none">
supersedes: <relative path or "none">
audience: user | maintainer | contributor
---
```

History docs use:

```yaml
---
status: historical | superseded
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---
```

Rules:

- `status: current` requires `updated` and either a build hash or an explicit
  `verified_build: none` with a reason.
- `diataxis` must match the containing directory.
- `history/` docs must never be linked as current instructions.
- A broken front matter block fails CI.

## 6. Status model

`docs/status/STATUS.json`:

```json
{
  "schema": 1,
  "updated": "2026-11-02",
  "code_commit": "<git sha or unknown>",
  "builds": {
    "gw_shell": "<sha256 or null>",
    "gw_system": "<sha256 or null>",
    "gwctl": "<sha256 or null>",
    "gw_appctl": "<sha256 or null>"
  },
  "device": {
    "profile": "pw3-0kb-5.16.2.1.1",
    "last_boot_id": "<uuid or null>"
  },
  "verified": {
    "reader_autostart": true,
    "power_button_suspend": true,
    "wifi_open": true,
    "wifi_wpa2": true,
    "new_ui_device_acceptance": false
  },
  "pending": [
    "new UI device acceptance",
    "generic app launch",
    "Tailscale SSH browser auth",
    "web upload"
  ],
  "current_evidence": "EVIDENCE-INDEX.md",
  "supersedes": "CURRENT.md"
}
```

`CURRENT.md` is generated from this file or checked against it. Manual prose can
explain consequences, but the facts come from the JSON.

`docs/status/EVIDENCE-INDEX.md` is generated from per-evidence metadata. Each
current evidence directory/file gets a small sidecar or front matter:

```yaml
---
date: 2026-11-02
scope: UI/UX hierarchy pass
build: <sha256>
boot_id: <uuid>
status: current
supersedes: ../../history/evidence/pw3-ui-refresh-2026-11-01
---
```

## 7. Phases

### Phase 0 — Inventory and freeze

- [x] Run a docs inventory: file count, category, status, owner, last update.
- [x] Record the current link-check baseline (27 broken relative links: 7 active,
      20 historical).
- [x] Freeze content edits except status/front matter during the move.
- [x] Decide which evidence sets are current vs historical:
      `pw3-ui-hierarchy-2026-11-02` current;
      `pw3-ui-refresh-2026-11-01` historical;
      `pw3-ui-polish-2026-11-01` historical;
      `paint-quality` historical/pre-policy.
- [x] Create `docs/history/README.md` with an old-path → new-path map.

**Exit gate:** inventory and current/historical decision are recorded; no files moved yet.

### Phase 1 — Status and docs checker first

- [x] Add `docs/status/STATUS.json` with the current facts.
- [x] Add `docs/status/CURRENT.md` generated/checked from it.
- [x] Add `docs/status/EVIDENCE-INDEX.md` (can start manually, then generate).
- [x] Add `tools/docs/check.py`:
  - front matter presence/shape;
  - internal link existence;
  - forbidden `/home/` and `/nix/store` outside `history/`;
  - `STATUS.json` schema keys;
  - table blank-line check for `BACKLOG.md`.
- [x] Add `markdownlint-cli2` and a local-link checker (`lychee` or
      `markdown-link-check`) to `tools/requirements-dev.txt`/CI.
- [x] Add a pre-push hook or extend `tools/hooks/pre-push` to run the docs check.

**Exit gate:** docs checker runs and fails on a deliberately broken link/front matter; current status is machine-readable.

### Phase 2 — Move current docs

- [x] Create `tutorials/`, `how-to/`, `reference/`, `explanation/`.
- [x] `git mv` the current docs in the mapping table.
- [x] Add front matter to each moved current doc.
- [x] Rewrite the top-level `docs/README.md` as the single entry index.
- [x] Update all relative links with a scripted mapping and re-run the checker.
- [x] Do not rewrite content yet; only front matter and links.

**Exit gate:** zero broken links in current docs; every current doc has valid front matter and a category.

### Phase 3 — Archive historical docs

- [x] `git mv` completed plans/specs/old technical investigations to `history/`.
- [x] Add `status: historical` or `superseded` banners.
- [x] Move `HANDOFF`, `MIGRATION`, old paint policy docs, old UI plans.
- [x] Add `docs/history/README.md` with the old-path map and “do not run” warning.
- [x] Keep `docs/history/BACKLOG.md` content and anchors intact.

**Exit gate:** no historical doc is in `tutorials/`, `how-to/`, `reference/`, or `explanation/` without an explicit historical banner.

### Phase 4 — Split backlog and evidence

- [x] Move open items to `docs/status/BACKLOG.md`; preserve B-IDs.
- [x] Move imported history to `docs/history/backlog-history.md`.
- [x] Fix the B13–B24 blank-line table break.
- [x] Create `docs/evidence/README.md` as the current evidence index.
- [x] Move superseded evidence sets to `docs/history/evidence/` or mark them in the index.
- [x] Add provenance metadata (date, build hash, boot ID, scope, supersedes).
- [x] Remove/redact `/home/will` and `/nix/store` from current evidence; keep
      raw private paths only in `history/` or work/private.

**Exit gate:** current evidence has one index; superseded evidence is clearly separated; backlog tables render correctly.

### Phase 5 — Content truth pass

- [x] Update `CURRENT` paint policy from `quality` to the current `smooth`/`GC16_FAST` behavior.
- [x] Update `CURRENT` screen names (`App manager`, one install/update path).
- [x] Update `OPERATIONS`:
  - remove “suspend not implemented”;
  - update Settings/App manager flow;
  - fix 15% → 25% headroom;
  - remove `0..24` formatting typos.
- [x] Update `EINK-UI-RENDERING` and `PW3-PAINT-POLICY` or mark them historical.
- [x] Update `docs/technical/README.md`:
  - remove obsolete `ours/scripts/...` commands to history;
  - link the current remediation and UI hierarchy plans;
  - fix the hardening “chưa ghi thiết bị” contradiction.
- [x] Replace `/home/will/dev/kindle` with `<repo>`/`$REPO` in current docs.

**Exit gate:** current docs agree with current code/build facts; historical docs are not quoted as current.

### Phase 6 — CI enforcement and sign-off

- [x] CI runs:
  - `tools/docs/check.py`;
  - `markdownlint-cli2 "docs/**/*.md"`;
  - local-link checker;
  - `STATUS.json` schema check;
  - forbidden-path grep.
- [x] CI fails if `CURRENT.md` is older than the latest build hash in `STATUS.json`.
- [x] Run the full host gate after doc-only changes.
- [x] Record the docs migration commit/tag and update B25.
- [x] Keep a rollback tag before the move.

**Exit gate:** docs CI is green on a fresh clone; a deliberately stale `CURRENT` or broken link fails.

## 8. Link and formatting policy

- No local link may point outside the repository unless it is explicitly marked
  `local-only` in code text.
- Historical references use code spans or `docs/history/README.md` redirects.
- Tables must have no blank lines between rows.
- One H1 per document; sections use H2/H3.
- Fenced code blocks must be balanced.
- Command examples use `<repo>`, `<work>`, `<nix-store>` placeholders, never
  `/home/will` or a host-specific store hash, except in `history/`.
- Evidence filenames include date and scope; the index carries build/boot provenance.

## 9. What not to do

- Do not rewrite all 20,000 lines in one PR.
- Do not delete old evidence or old plans; history is append-only.
- Do not create duplicate “new” copies beside old docs.
- Do not add a static-site framework before the structure and links are stable.
- Do not move docs and change technical conclusions in the same commit.
- Do not use prose “Status: complete” without machine-checkable front matter.

## 10. Acceptance checklist

- [x] `docs/` contains only `tutorials/`, `how-to/`, `reference/`,
      `explanation/`, `status/`, `evidence/`, `history/` plus `README.md`.
- [x] Every current doc has valid front matter and a `diataxis` category.
- [x] `docs/status/STATUS.json` is the only source of current build/status facts.
- [x] `CURRENT.md` is generated or checked from `STATUS.json`.
- [x] `EVIDENCE-INDEX.md` lists every current evidence set with build/boot hash.
- [x] Zero broken local links in current docs.
- [x] Zero `/home/` or `/nix/store` paths outside `history/`.
- [x] `BACKLOG.md` renders as one table; B-IDs are preserved.
- [x] Historical docs are under `history/` with banners.
- [x] Docs CI passes in a fresh clone.
- [x] B25 closed only after the CI gate is enforced.

## 11. Risks and rollback

| Risk | Mitigation |
| --- | --- |
| Moves break internal links | Scripted mapping + link checker; move in small batches |
| History is lost or rewritten | `git mv`; no deletes; history append-only |
| Current/historical boundary remains fuzzy | Front matter + CI gate |
| Evidence directory churn is large | Index first, move in batches; keep old evidence in history |
| Docs work hides code changes | Doc-only PRs; no source behavior edits |
| Tooling adds friction | Start with Python link/front-matter checker; add Markdown/link binaries only after baseline |

Rollback: each phase is a separate commit/tag. Revert the phase, not the whole
migration.

## 12. Recommended first three PRs

1. **`docs/status-v1`**
   - Add `STATUS.json`, `CURRENT.md`, `EVIDENCE-INDEX.md`.
   - Add `tools/docs/check.py` and CI link/front-matter checks.
   - No file moves.

2. **`docs/current-tree-v1`**
   - Create `tutorials/`, `how-to/`, `reference/`, `explanation/`.
   - `git mv` current docs + front matter.
   - Update links.

3. **`docs/history-v1`**
   - Move old plans, MIGRATION/HANDOFF, old paint policy, old evidence.
   - Add history index/old-path map.
   - Split backlog open vs history.

After those three, the remaining work is content truth fixes and the final
enforcement pass.
