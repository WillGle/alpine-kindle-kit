---
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# Current evidence

Current evidence is indexed in the generated
[`../status/EVIDENCE-INDEX.md`](../status/EVIDENCE-INDEX.md). That index carries
the date, scope, build hash and boot ID for every current set.

Superseded evidence is preserved under
[`../history/evidence/`](../history/evidence/) and is reachable from the
[history index](../history/README.md). It is not current acceptance.

## Rules

- Evidence files are append-only. Do not rewrite a measurement to match a newer
  build; add a new set and mark the old one superseded.
- A framebuffer capture proves what was written to the framebuffer, not what the
  physical panel showed. State that limit in the note.
- Current evidence must not contain host-specific home or package-store
  paths, tokens, keys, serials or private session data.
- Add new sets to [`INDEX.json`](INDEX.json), then run
  `python3 tools/docs/check.py --write`.

See [`../how-to/capture-evidence.md`](../how-to/capture-evidence.md) for the
capture procedure.
