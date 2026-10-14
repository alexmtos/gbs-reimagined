"""Settings and translations: schema/settings.yaml + schema/i18n/*.yaml -> firmware, web, docs.

Called by generate.py. Validation failures raise SystemExit with every problem listed, so a
missing translation or an over-long OLED string fails the build instead of shipping.
"""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

import yaml

LABEL_MAX = 20  # OLED line at the menu font, with the cursor
HINT_MAX = 21  # OLED bottom line
OLED_LINE = 21  # characters per OLED line (6x10 font, 128 px)
OLED_PREFIX = "oled."
OLED_TWO_LINES = "oled.headline."  # status sentences may wrap onto a second line
REFERENCE_LOCALE = "en"
FLAG_RISKY = 0x01
FLAG_ADVANCED = 0x02
MILESTONE_SHIFT = 4  # flags bits 4..5: milestone 1..3
FLAG_PENDING = 0x40


def fnv1a(text: str) -> int:
    h = 0x811C9DC5
    for byte in text.encode("utf-8"):
        h ^= byte
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def c_string(text: str) -> str:
    out = []
    for ch in text:
        if ch in '"\\':
            out.append("\\" + ch)
        elif ord(ch) < 0x20:
            # Octal, not hex: a hex escape absorbs the letters that follow it ("\x1fRGB").
            out.append(f"\\{ord(ch):03o}")
        else:
            out.append(ch)
    return "".join(out)


def ident(setting_id: str) -> str:
    """output.frameLock -> OutputFrameLock"""
    parts = setting_id.replace(".", " ").split()
    return "".join(p[0].upper() + p[1:] for p in parts)


class Blob:
    """NUL-separated strings in one flash array, deduplicated, addressed by offset."""

    def __init__(self) -> None:
        self.parts: list[str] = []
        self.offsets: dict[str, int] = {}
        self.size = 0

    def add(self, text: str) -> int:
        if text not in self.offsets:
            self.offsets[text] = self.size
            self.parts.append(text)
            self.size += len(text.encode("utf-8")) + 1
        return self.offsets[text]

    def emit(self, name: str) -> list[str]:
        lines = [f"const char {name}[] PROGMEM ="]
        for part in self.parts:
            lines.append(f'    "{c_string(part)}\\0"')
        if not self.parts:
            lines.append('    ""')
        lines[-1] += ";"
        return lines


def load(schema_dir: Path) -> tuple[dict, dict[str, dict]]:
    with (schema_dir / "settings.yaml").open(encoding="utf-8") as fh:
        schema = yaml.safe_load(fh)
    locales: dict[str, dict] = {}
    for path in sorted((schema_dir / "i18n").glob("*.yaml")):
        with path.open(encoding="utf-8") as fh:
            data = yaml.safe_load(fh)
        locales[data["locale"]] = data
    return schema, locales


def special_ids(setting: dict) -> list[tuple[int, str]]:
    return sorted((int(k), str(v)) for k, v in (setting.get("special") or {}).items())


def validate(schema: dict, locales: dict[str, dict]) -> list[str]:
    problems: list[str] = []
    settings: dict = schema["settings"]
    if not re.fullmatch(r"M[123]", str(schema.get("current_milestone", ""))):
        problems.append("current_milestone must be M1, M2 or M3")
    if REFERENCE_LOCALE not in locales:
        problems.append(f"missing reference locale {REFERENCE_LOCALE}")
    hashes: dict[int, str] = {}
    for sid, s in settings.items():
        h = fnv1a(sid)
        if h in hashes:
            problems.append(f"{sid}: hash collides with {hashes[h]}")
        hashes[h] = sid
        group = sid.split(".")[0]
        if group not in schema["groups"]:
            problems.append(f"{sid}: unknown group {group}")
        t = s.get("type")
        if t == "enum":
            if s["default"] not in s["values"]:
                problems.append(f"{sid}: default {s['default']!r} not in values")
        elif t == "int":
            if not s["min"] <= s["default"] <= s["max"]:
                problems.append(f"{sid}: default out of range")
            if s["min"] < -32768 or s["max"] > 32767:
                problems.append(f"{sid}: range exceeds int16")
        elif t == "bool":
            if not isinstance(s["default"], bool):
                problems.append(f"{sid}: bool default must be true/false")
        else:
            problems.append(f"{sid}: unknown type {t!r}")
        for field in ("tier", "milestone"):
            if field not in s:
                problems.append(f"{sid}: missing {field}")
        if "pending" in s and s["pending"] is not True:
            problems.append(f"{sid}: pending is either true or absent")
    for loc, data in locales.items():
        strings = data.get("settings", {})
        for group in schema["groups"]:
            if group not in data.get("groups", {}):
                problems.append(f"[{loc}] group {group}: no name")
        for sid, s in settings.items():
            entry = strings.get(sid)
            if entry is None:
                problems.append(f"[{loc}] {sid}: no strings")
                continue
            for key in ("label", "hint", "help"):
                if not entry.get(key):
                    problems.append(f"[{loc}] {sid}: missing {key}")
            if len(entry.get("label", "")) > LABEL_MAX:
                problems.append(f"[{loc}] {sid}: label longer than {LABEL_MAX}")
            if len(entry.get("hint", "")) > HINT_MAX:
                problems.append(f"[{loc}] {sid}: hint longer than {HINT_MAX}")
            wanted = list(s["values"]) if s["type"] == "enum" else []
            wanted += [name for _, name in special_ids(s)]
            for value in wanted:
                if value not in (entry.get("values") or {}):
                    problems.append(f"[{loc}] {sid}: no label for value {value!r}")
        extra = set(strings) - set(settings)
        for sid in sorted(extra):
            problems.append(f"[{loc}] {sid}: strings for an unknown setting")
    reference_messages = set((locales.get(REFERENCE_LOCALE) or {}).get("messages") or {})
    for loc, data in locales.items():
        messages = set(data.get("messages") or {})
        for key in sorted(reference_messages - messages):
            problems.append(f"[{loc}] message {key}: missing")
        for key in sorted(messages - reference_messages):
            problems.append(f"[{loc}] message {key}: not in the reference locale")
        for key, text in (data.get("messages") or {}).items():
            if not key.startswith(OLED_PREFIX):
                continue
            limit = 2 * OLED_LINE if key.startswith(OLED_TWO_LINES) else OLED_LINE
            if len(text) > limit:
                problems.append(f"[{loc}] message {key}: longer than {limit} for the OLED")
        # Everything the OLED shows must exist in its Latin-1 font.
        shown = [m for k, m in (data.get("messages") or {}).items() if k.startswith(OLED_PREFIX)]
        for entry in (data.get("settings") or {}).values():
            shown += [entry.get("label", ""), entry.get("hint", "")]
            shown += list((entry.get("values") or {}).values())
        shown += list((data.get("groups") or {}).values())
        for text in shown:
            try:
                text.encode("latin-1")
            except UnicodeEncodeError:
                problems.append(f"[{loc}] {text!r}: not displayable on the OLED (Latin-1 only)")
    return problems


def oled_keys(locales: dict[str, dict]) -> list[str]:
    messages = (locales.get(REFERENCE_LOCALE) or {}).get("messages") or {}
    return [k for k in messages if k.startswith(OLED_PREFIX)]


def msg_ident(key: str) -> str:
    parts = key[len(OLED_PREFIX) :].replace("-", ".").split(".")
    return "".join(p[:1].upper() + p[1:] for p in parts)


def schema_hash(schema: dict) -> str:
    canonical = json.dumps(schema, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:8]


def milestone_number(s: dict) -> int:
    return int(str(s["milestone"]).lstrip("M"))


def current_milestone(schema: dict) -> int:
    return int(str(schema["current_milestone"]).lstrip("M"))


def is_available(schema: dict, s: dict) -> bool:
    return milestone_number(s) <= current_milestone(schema) and not s.get("pending")


def generate(root: Path, banner) -> dict[Path, str]:
    schema, locales = load(root / "schema")
    problems = validate(schema, locales)
    if problems:
        raise SystemExit("settings schema invalid:\n  " + "\n  ".join(problems))

    settings: dict = schema["settings"]
    ids = list(settings)
    shash = schema_hash(schema)
    src = "schema/settings.yaml and schema/i18n/*.yaml"
    out: dict[Path, str] = {}

    # ---------------------------------------------------------------- firmware: definitions
    names = Blob()
    values_blob = Blob()
    rows: list[str] = []
    for sid in ids:
        s = settings[sid]
        flags = FLAG_RISKY if s.get("risky") else 0
        flags |= FLAG_ADVANCED if s["tier"] == "advanced" else 0
        flags |= milestone_number(s) << MILESTONE_SHIFT
        flags |= FLAG_PENDING if s.get("pending") else 0
        if s["type"] == "enum":
            vmin, vmax, vdef = 0, len(s["values"]) - 1, s["values"].index(s["default"])
            # Trailing NUL plus the blob's own terminator = the empty string that ends the list.
            value_list = "\0".join(s["values"]) + "\0"
            kind = "Type::Enum"
        elif s["type"] == "bool":
            vmin, vmax, vdef = 0, 1, int(s["default"])
            value_list = ""
            kind = "Type::Bool"
        else:
            vmin, vmax, vdef = s["min"], s["max"], s["default"]
            value_list = ""
            kind = "Type::Int"
        specials = special_ids(s)
        sp = [str(v) for v, _ in specials] + ["kNoSpecial"] * (2 - len(specials))
        voff = values_blob.add(value_list) if value_list else 0xFFFF
        rows.append(
            f"    {{0x{fnv1a(sid):08X}u, {names.add(sid)}, {voff}, {vmin}, {vmax}, {vdef},"
            f" {{{', '.join(sp)}}}, {kind}, 0x{flags:02X}}}, // {sid}"
        )
    hdr = [banner(src), "#pragma once", "", "#include <stdint.h>", ""]
    hdr += ["namespace gbsr::settings {", "", "enum class Id : uint8_t {"]
    hdr += [f"    {ident(sid)}," for sid in ids]
    hdr += ["    Count,", "};", ""]
    hdr += [f'inline constexpr char kSchemaHash[] = "{shash}";']
    hdr += [f"inline constexpr uint8_t kGroupCount = {len(schema['groups'])};"]
    hdr += ["// The milestone being built; later milestones' settings are generated but hidden."]
    hdr += [f"inline constexpr uint8_t kMilestone = {current_milestone(schema)};"]
    hdr += ["", "} // namespace gbsr::settings", ""]
    out[root / "firmware/src/core/settings.gen.h"] = "\n".join(hdr)

    body = [banner(src), '#include "core/settings_def.h"', "", "namespace gbsr::settings {", ""]
    body += ["// Setting ids, NUL-separated (for the console and persistence diagnostics)."]
    body += names.emit("kIdNames") + [""]
    body += ["// Enum value ids per setting, NUL-separated; each list ends at an empty string."]
    body += values_blob.emit("kValueIds") + [""]
    body += ["const Def kDefs[] PROGMEM = {"] + rows + ["};", ""]
    body += ["} // namespace gbsr::settings", ""]
    out[root / "firmware/src/core/settings.gen.cpp"] = "\n".join(body)

    # ---------------------------------------------------------------- firmware: OLED strings
    sbody = [banner(src), '#include "ui/strings.h"', "", "namespace gbsr::i18n {", ""]
    tables: list[str] = []
    for loc, data in locales.items():
        tag = loc.replace("-", "")
        blob = Blob()
        label_off, hint_off, val_off = [], [], []
        for sid in ids:
            s = settings[sid]
            e = data["settings"][sid]
            label_off.append(blob.add(e["label"]))
            hint_off.append(blob.add(e["hint"]))
            labels = [e["values"][v] for v in s["values"]] if s["type"] == "enum" else []
            labels += [e["values"][name] for _, name in special_ids(s)]
            val_off.append(blob.add("\x1f".join(labels)) if labels else 0xFFFF)
        group_off = [blob.add(data["groups"][g]) for g in schema["groups"]]
        msg_off = [blob.add(data["messages"][k]) for k in oled_keys(locales)]
        sbody += blob.emit(f"kText_{tag}")
        for name, values in (
            ("Label", label_off),
            ("Hint", hint_off),
            ("Values", val_off),
            ("Group", group_off),
            ("Msg", msg_off),
        ):
            sbody.append(
                f"const uint16_t k{name}_{tag}[] PROGMEM = {{{', '.join(map(str, values))}}};"
            )
        sbody.append("")
        names = ("Text", "Label", "Hint", "Values", "Group", "Msg")
        fields = ", ".join(f"k{n}_{tag}" for n in names)
        tables.append(f"    {{{fields}}},")
    sbody += ["const LocaleTables kLocales[] PROGMEM = {"] + tables + ["};", ""]
    sbody += [f"const uint8_t kLocaleCount = {len(locales)};", ""]
    sbody += ["} // namespace gbsr::i18n", ""]
    out[root / "firmware/src/ui/strings.gen.cpp"] = "\n".join(sbody)

    mbody = [banner(src), "#pragma once", "", "#include <stdint.h>", ""]
    mbody += ["namespace gbsr::i18n {", ""]
    mbody += ["// OLED messages (schema/i18n, keys oled.*), in the order of the tables' `msg`."]
    mbody += ["enum class Msg : uint8_t {"]
    mbody += [f"    {msg_ident(k)}, // {k}" for k in oled_keys(locales)]
    mbody += ["    Count,", "};", "", "} // namespace gbsr::i18n", ""]
    out[root / "firmware/src/ui/messages.gen.h"] = "\n".join(mbody)

    # ---------------------------------------------------------------- web
    meta = []
    for sid in ids:
        s = settings[sid]
        m = {
            "id": sid,
            "group": sid.split(".")[0],
            "type": s["type"],
            "default": s["default"],
            "tier": s["tier"],
            "milestone": milestone_number(s),
            "available": is_available(schema, s),
            "risky": bool(s.get("risky")),
        }
        if s["type"] == "enum":
            m["values"] = s["values"]
        if s["type"] == "int":
            m["min"], m["max"] = s["min"], s["max"]
            if s.get("special"):
                m["special"] = {str(k): v for k, v in s["special"].items()}
        meta.append(m)
    ts = [banner(src)]
    ts += ["export type SettingType = 'enum' | 'int' | 'bool';", ""]
    ts += ["export interface SettingMeta {"]
    ts += ["  id: SettingId;", "  group: SettingGroup;", "  type: SettingType;"]
    ts += ["  default: string | number | boolean;", "  tier: 'essential' | 'advanced';"]
    ts += ["  milestone: number;", "  available: boolean;", "  risky: boolean;"]
    ts += ["  values?: readonly string[];"]
    ts += ["  min?: number;", "  max?: number;", "  special?: Readonly<Record<string, string>>;"]
    ts += ["}", ""]
    ts += [f"export const SCHEMA_HASH = '{shash}';", ""]
    ts += ["export const SETTING_GROUPS = " + json.dumps(schema["groups"]) + " as const;"]
    ts += ["export type SettingGroup = (typeof SETTING_GROUPS)[number];", ""]
    ts += ["export const SETTING_IDS = " + json.dumps(ids, indent=2) + " as const;"]
    ts += ["export type SettingId = (typeof SETTING_IDS)[number];", ""]
    ts += ["export const SETTINGS: readonly SettingMeta[] = " + json.dumps(meta, indent=2) + ";"]
    ts += [""]
    out[root / "web/src/gen/settings.gen.ts"] = "\n".join(ts)

    for loc, data in locales.items():
        payload = {"locale": loc, "name": data["name"], "groups": data["groups"]}
        payload["settings"] = data["settings"]
        payload["messages"] = data.get("messages", {})
        out[root / f"web/src/gen/i18n/{loc}.gen.json"] = (
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
        )

    # ---------------------------------------------------------------- docs
    en = locales[REFERENCE_LOCALE]
    md = [
        f"<!-- GENERATED by tools/codegen/generate.py from {src}. Do not edit by hand. -->",
        "",
        "# Settings reference",
        "",
        f"Schema hash `{shash}`. Every user-facing setting, as defined in `schema/settings.yaml`;"
        " labels and help are the English strings. *Risky* settings arm the timed revert.",
        "",
    ]
    for group in schema["groups"]:
        md += [
            f"## {en['groups'][group]}",
            "",
            "| Id | Label | Values | Default | Tier | Ms | Help |",
        ]
        md += ["|---|---|---|---|---|---|---|"]
        for sid in ids:
            if not sid.startswith(group + "."):
                continue
            s, e = settings[sid], en["settings"][sid]
            if s["type"] == "enum":
                values = ", ".join(e["values"][v] for v in s["values"])
                default = e["values"][s["default"]]
            elif s["type"] == "bool":
                values, default = "off, on", "on" if s["default"] else "off"
            else:
                values = f"{s['min']}…{s['max']}"
                for v, name in special_ids(s):
                    values += f" ({v} = {e['values'][name]})"
                default = str(s["default"])
            risky = " ⚠" if s.get("risky") else ""
            md.append(
                f"| `{sid}`{risky} | {e['label']} | {values} | {default} | {s['tier']} |"
                f" {s['milestone']}{' (pending)' if s.get('pending') else ''} | {e['help']} |"
            )
        md.append("")
    out[root / "docs/reference/settings.md"] = "\n".join(md)
    return out
