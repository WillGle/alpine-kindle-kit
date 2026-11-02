#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Documentation contract checker for the Diátaxis layout.

Checks, without third-party Python packages:

* the top-level docs tree contains only the maintained categories;
* every current document has valid front matter and a category;
* every history document is explicitly marked historical/superseded;
* local Markdown links in current documents resolve;
* current documents do not contain host-specific home or package-store paths;
* STATUS.json has the required schema;
* CURRENT.md and EVIDENCE-INDEX.md match generated output;
* BACKLOG tables do not contain blank lines between rows.

Use --write to regenerate CURRENT.md and EVIDENCE-INDEX.md.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
STATUS_DIR = DOCS / "status"
STATUS_JSON = STATUS_DIR / "STATUS.json"
CURRENT_MD = STATUS_DIR / "CURRENT.md"
EVIDENCE_INDEX = STATUS_DIR / "EVIDENCE-INDEX.md"
EVIDENCE_JSON = DOCS / "evidence" / "INDEX.json"
BACKLOG = STATUS_DIR / "BACKLOG.md"

CATEGORIES = {"tutorials", "how-to", "reference", "explanation"}
TOP_LEVEL_DIRS = CATEGORIES | {"status", "evidence", "history"}
FRONT_MATTER_KEYS = ("status", "updated", "verified_build", "supersedes",
                     "audience")
HISTORY_KEYS = ("status", "date", "superseded_by")
STATUS_KEYS = (
    "schema", "updated", "code_commit", "builds", "device", "verified",
    "pending", "current_evidence", "supersedes",
)
BUILD_KEYS = ("gw_shell", "gw_system", "gwctl", "gw_appctl")
VERIFIED_KEYS = (
    "reader_autostart", "power_button_suspend", "wifi_open", "wifi_wpa2",
    "remediation_ui_device_acceptance", "f_ui_hierarchy_device_acceptance",
    "lock_screen_device_trial", "lock_screen_plugin_menu",
)
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
FORBIDDEN = ("/" + "home" + "/", "/" + "nix" + "/store")

errors: list[str] = []


def fail(message: str) -> None:
    errors.append(message)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_front_matter(path: Path) -> tuple[dict[str, str] | None, str]:
    text = read(path)
    if not text.startswith("---"):
        return None, text
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return None, text
    end = None
    for index in range(1, len(lines)):
        if lines[index].strip() == "---":
            end = index
            break
    if end is None:
        return None, text
    data: dict[str, str] = {}
    for line in lines[1:end]:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if ":" not in line:
            fail(f"{path.relative_to(ROOT)}: malformed front matter line: {line!r}")
            continue
        key, value = line.split(":", 1)
        data[key.strip()] = value.strip().strip('"').strip("'")
    return data, "\n".join(lines[end + 1:]).lstrip("\n")


def strip_code(text: str) -> str:
    """Remove fenced blocks and inline code so examples are not link-checked."""
    out: list[str] = []
    fenced = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            fenced = not fenced
            out.append("")
            continue
        out.append("" if fenced else re.sub(r"`[^`]*`", "", line))
    return "\n".join(out)


def relative_link_target(source: Path, raw: str) -> str | None:
    target = raw.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    if not target or target.startswith("#"):
        return None
    if SCHEME_RE.match(target):
        return None
    target = target.split("#", 1)[0].split("?", 1)[0]
    if not target:
        return None
    # Keep URLs with percent-encoding readable for the filesystem check.
    from urllib.parse import unquote
    return unquote(target)


def check_links(path: Path, text: str) -> None:
    for match in LINK_RE.finditer(strip_code(text)):
        target = relative_link_target(path, match.group(1))
        if target is None:
            continue
        resolved = (path.parent / target).resolve()
        if not resolved.exists():
            fail(f"{path.relative_to(ROOT)}: broken local link: {match.group(1)}")


def check_forbidden(path: Path, text: str) -> None:
    for token in FORBIDDEN:
        if token in text:
            fail(f"{path.relative_to(ROOT)}: forbidden host path {token!r}")


def check_current_front_matter(path: Path, data: dict[str, str] | None) -> None:
    rel = path.relative_to(ROOT)
    if data is None:
        fail(f"{rel}: missing front matter")
        return
    for key in FRONT_MATTER_KEYS:
        if key not in data:
            fail(f"{rel}: front matter missing {key!r}")
    if data.get("status") not in {"current", "draft", "deprecated"}:
        fail(f"{rel}: invalid front matter status {data.get('status')!r}")
    if not DATE_RE.match(data.get("updated", "")):
        fail(f"{rel}: updated must be YYYY-MM-DD")
    build = data.get("verified_build", "")
    if build != "none" and not re.match(r"^[0-9a-f]{64}$", build):
        fail(f"{rel}: verified_build must be a sha256 or 'none'")
    if "supersedes" not in data:
        fail(f"{rel}: supersedes must be present (use 'none')")
    if data.get("audience") not in {"user", "maintainer", "contributor"}:
        fail(f"{rel}: audience must be user, maintainer or contributor")
    category = path.relative_to(DOCS).parts[0]
    expected = {"tutorials": "tutorial"}.get(category, category)
    if category in CATEGORIES and data.get("diataxis") != expected:
        fail(f"{rel}: diataxis must match directory {category!r}")


def check_history_front_matter(path: Path, data: dict[str, str] | None) -> None:
    rel = path.relative_to(ROOT)
    if data is None:
        fail(f"{rel}: history document is missing front matter")
        return
    for key in HISTORY_KEYS:
        if key not in data:
            fail(f"{rel}: history front matter missing {key!r}")
    if data.get("status") not in {"historical", "superseded"}:
        fail(f"{rel}: history status must be historical or superseded")
    if not DATE_RE.match(data.get("date", "")):
        fail(f"{rel}: history date must be YYYY-MM-DD")


def check_tree() -> list[Path]:
    current: list[Path] = []
    for entry in sorted(DOCS.iterdir()):
        if entry.is_dir():
            if entry.name not in TOP_LEVEL_DIRS:
                fail(f"docs/: unexpected directory {entry.name!r}")
        elif entry.name != "README.md":
            fail(f"docs/: unexpected top-level file {entry.name!r}")
    for path in sorted(DOCS.rglob("*.md")):
        rel_parts = path.relative_to(DOCS).parts
        is_history = rel_parts[0] == "history"
        data, body = parse_front_matter(path)
        if is_history:
            check_history_front_matter(path, data)
            continue
        current.append(path)
        check_current_front_matter(path, data)
        check_links(path, body)
        check_forbidden(path, read(path))
    return current


def check_status_schema() -> dict:
    try:
        status = json.loads(read(STATUS_JSON))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"docs/status/STATUS.json: {exc}")
        return {}
    for key in STATUS_KEYS:
        if key not in status:
            fail(f"STATUS.json: missing key {key!r}")
    if status.get("schema") != 1:
        fail("STATUS.json: schema must be 1")
    if not DATE_RE.match(str(status.get("updated", ""))):
        fail("STATUS.json: updated must be YYYY-MM-DD")
    builds = status.get("builds")
    if not isinstance(builds, dict):
        fail("STATUS.json: builds must be an object")
    else:
        for key in BUILD_KEYS:
            if key not in builds:
                fail(f"STATUS.json: builds missing {key!r}")
            elif builds[key] is not None and not re.match(
                    r"^[0-9a-f]{64}$", str(builds[key])):
                fail(f"STATUS.json: builds.{key} must be null or sha256")
    device = status.get("device")
    if not isinstance(device, dict) or "profile" not in device or "last_boot_id" not in device:
        fail("STATUS.json: device must contain profile and last_boot_id")
    verified = status.get("verified")
    if not isinstance(verified, dict):
        fail("STATUS.json: verified must be an object")
    else:
        for key in VERIFIED_KEYS:
            if not isinstance(verified.get(key), bool):
                fail(f"STATUS.json: verified.{key} must be boolean")
    if not isinstance(status.get("pending"), list):
        fail("STATUS.json: pending must be an array")
    return status


def render_current(status: dict) -> str:
    builds = status.get("builds", {})
    verified = status.get("verified", {})
    pending = status.get("pending", [])

    def flag(value: object) -> str:
        return "yes" if value is True else "no" if value is False else "unknown"

    def build(value: object) -> str:
        return f"`{value}`" if value else "unknown"

    lines = [
        "---",
        "status: current",
        f"updated: {status.get('updated', 'unknown')}",
        "verified_build: none",
        "supersedes: none",
        "audience: maintainer",
        "---",
        "",
        "# Current status",
        "",
        "<!-- Generated by tools/docs/check.py --write from STATUS.json. -->",
        "<!-- Do not edit this file by hand. -->",
        "",
        f"Updated: **{status.get('updated', 'unknown')}**  ",
        f"Code commit: `{status.get('code_commit', 'unknown')}`  ",
        f"Device profile: `{status.get('device', {}).get('profile', 'unknown')}`  ",
        f"Last boot ID: `{status.get('device', {}).get('last_boot_id') or 'unknown'}`",
        "",
        "## Builds",
        "",
        "| Binary | SHA256 |",
        "| --- | --- |",
        f"| `gw_shell` | {build(builds.get('gw_shell'))} |",
        f"| `gw_system` | {build(builds.get('gw_system'))} |",
        f"| `gwctl` | {build(builds.get('gwctl'))} |",
        f"| `gw_appctl` | {build(builds.get('gw_appctl'))} |",
        "",
        "## Verified on hardware",
        "",
        "| Capability | Status |",
        "| --- | --- |",
        f"| Reader autostart | {flag(verified.get('reader_autostart'))} |",
        f"| Power-button suspend | {flag(verified.get('power_button_suspend'))} |",
        f"| Wi-Fi open network | {flag(verified.get('wifi_open'))} |",
        f"| Wi-Fi WPA2 | {flag(verified.get('wifi_wpa2'))} |",
        f"| A–E remediation UI device acceptance | {flag(verified.get('remediation_ui_device_acceptance'))} |",
        f"| F UI/UX hierarchy device acceptance | {flag(verified.get('f_ui_hierarchy_device_acceptance'))} |",
        f"| Lock screen device trial | {flag(verified.get('lock_screen_device_trial'))} |",
        f"| Lock screen plugin menu | {flag(verified.get('lock_screen_plugin_menu'))} |",
        "",
        "## Current display and UI policy",
        "",
        "- Partial updates use `GC16_FAST`; scroll release uses a partial `GC16`.",
        "- Full `GC16` is reserved for lock/unlock, the first Home after boot and",
        "  explicit idle cleanup.",
        "- Settings has one **App manager** entry; installed apps use **Refresh**,",
        "  and USB packages use one **Scan USB** and one **Install selected**",
        "  action.",
        "- The mandatory rules are in",
        "  [`../reference/display-programming-rules.md`](../reference/display-programming-rules.md).",
        "",
        "## Pending",
        "",
    ]
    if pending:
        lines.extend(f"- {item}" for item in pending)
    else:
        lines.append("- None recorded.")
    lines += [
        "",
        "## Provenance",
        "",
        "- [EVIDENCE-INDEX.md](EVIDENCE-INDEX.md) — current evidence with build and boot IDs.",
        "- [BACKLOG.md](BACKLOG.md) — open and tracked work items.",
        "- [../reference/device-profile-pw3.md](../reference/device-profile-pw3.md) — supported device profile.",
        "- Historical detail: [../history/status/CURRENT-2026-10-31.vi.md](../history/status/CURRENT-2026-10-31.vi.md).",
        "",
        "`verified_build: none` in a document means its instructions are not tied to a",
        "specific binary; the build facts above remain the only current source.",
        "",
    ]
    return "\n".join(lines)


def render_evidence_index() -> str:
    try:
        index = json.loads(read(EVIDENCE_JSON))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"docs/evidence/INDEX.json: {exc}")
        return ""
    entries = index.get("evidence")
    if not isinstance(entries, list):
        fail("docs/evidence/INDEX.json: evidence must be an array")
        return ""
    lines = [
        "---",
        "status: current",
        f"updated: {index.get('updated', 'unknown')}",
        "verified_build: none",
        "supersedes: none",
        "audience: maintainer",
        "---",
        "",
        "# Current evidence index",
        "",
        "<!-- Generated by tools/docs/check.py --write from docs/evidence/INDEX.json. -->",
        "",
        "Only current evidence is listed. Superseded sets live under",
        "[`history/evidence/`](../history/evidence/), reachable through the",
        "[history index](../history/README.md).",
        "",
        "| Date | Evidence | Scope | Build | Boot ID | Status |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for entry in entries:
        path = entry.get("path", "")
        target = (DOCS / "evidence" / path)
        if not target.exists():
            fail(f"docs/evidence/INDEX.json: missing evidence path {path!r}")
        link = f"[`{path}`](../evidence/{path})"
        build = entry.get("build") or "none"
        boot = entry.get("boot_id") or "unknown"
        lines.append(
            f"| {entry.get('date', 'unknown')} | {link} | {entry.get('scope', '')} "
            f"| `{build}` | `{boot}` | {entry.get('status', 'current')} |"
        )
    lines.append("")
    return "\n".join(lines)


def check_generated() -> None:
    status = check_status_schema()
    if status:
        expected = render_current(status)
        if not CURRENT_MD.exists():
            fail("docs/status/CURRENT.md: missing")
        elif read(CURRENT_MD) != expected:
            fail("docs/status/CURRENT.md: out of date; run tools/docs/check.py --write")
    expected_index = render_evidence_index()
    if expected_index:
        if not EVIDENCE_INDEX.exists():
            fail("docs/status/EVIDENCE-INDEX.md: missing")
        elif read(EVIDENCE_INDEX) != expected_index:
            fail("docs/status/EVIDENCE-INDEX.md: out of date; run "
                 "tools/docs/check.py --write")


def check_backlog_tables() -> None:
    if not BACKLOG.exists():
        fail("docs/status/BACKLOG.md: missing")
        return
    lines = read(BACKLOG).splitlines()
    previous_table = False
    for index, line in enumerate(lines):
        stripped = line.strip()
        is_table = stripped.startswith("|")
        if (is_table and previous_table is False and index > 0
                and lines[index - 1].strip() == "" and index >= 2
                and lines[index - 2].strip().startswith("|")):
            fail(f"docs/status/BACKLOG.md:{index + 1}: blank line inside a table")
        previous_table = is_table


def write_generated() -> None:
    status = json.loads(read(STATUS_JSON))
    CURRENT_MD.write_text(render_current(status), encoding="utf-8")
    EVIDENCE_INDEX.write_text(render_evidence_index(), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true",
                        help="regenerate CURRENT.md and EVIDENCE-INDEX.md")
    args = parser.parse_args()
    check_tree()
    check_backlog_tables()
    if args.write:
        write_generated()
        errors.clear()
        check_tree()
        check_backlog_tables()
    check_generated()
    if errors:
        for error in errors:
            print(f"DOCS FAIL: {error}", file=sys.stderr)
        print(f"{len(errors)} documentation error(s)", file=sys.stderr)
        return 1
    print("DOCS CHECK PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
