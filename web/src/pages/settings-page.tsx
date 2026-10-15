import { useEffect, useState } from "preact/hooks";
import { fetchBackup, runCommand } from "../api/endpoints";
import { Backup } from "../components/backup";
import { FirmwareUpdate } from "../components/firmware-update";
import { bad, info, Note, type Notice, warn } from "../components/note";
import { downloadJson } from "../download";
import type { SettingGroup, SettingMeta } from "../gen/settings.gen";
import { SETTINGS } from "../gen/settings.gen";
import { settingText, t, valueLabel } from "../i18n";
import { defaultText, ELSEWHERE, idleReason, offered, SECTIONS } from "../settings/layout";
import { profileName } from "../state/profiles";
import {
  changeSetting,
  fieldState,
  loaded,
  setShowAdvanced,
  showAdvanced,
  values,
} from "../state/settings";
import { status } from "../state/status";

/** Integers with named values and a short range read better as a list than as a slider. */
const LIST_RANGE = 20;

/** The text for a number in a setting's list (the startup profile shows the profile's name). */
function numberLabel(meta: SettingMeta, n: number): string {
  if (meta.id === "system.startupProfile") {
    const name = profileName(n);
    return name ? `${n} · ${name}` : t("profiles.slotFree", { slot: n });
  }
  return (meta.min ?? 0) < 0 && n > 0 ? `+${n}` : String(n);
}

/** The number behind a value text: a special value's name maps back to its number. */
function numberOf(meta: SettingMeta, text: string): number {
  const special = Object.entries(meta.special ?? {}).find(([, name]) => name === text);
  return special ? Number(special[0]) : Number(text);
}

/** The control for one setting; the board's answer decides what it finally shows. */
export function Control({ meta }: { meta: SettingMeta }) {
  const value = values.value[meta.id];
  const [dragging, setDragging] = useState<string | null>(null);
  const label = settingText(meta.id).label;
  if (value === undefined) return <span class="muted">—</span>;
  switch (meta.type) {
    case "enum":
      return (
        <select
          aria-label={label}
          value={value}
          onChange={(e) => void changeSetting(meta.id, e.currentTarget.value)}
        >
          {(meta.values ?? [])
            .filter((v) => offered(meta.id, v, value, showAdvanced.value))
            .map((v) => (
              <option key={v} value={v}>
                {valueLabel(meta.id, v)}
              </option>
            ))}
        </select>
      );
    case "bool":
      return (
        <input
          type="checkbox"
          role="switch"
          aria-checked={value === "on"}
          aria-label={label}
          checked={value === "on"}
          onChange={(e) => void changeSetting(meta.id, e.currentTarget.checked ? "on" : "off")}
        />
      );
    default: {
      const min = meta.min ?? 0;
      const max = meta.max ?? 0;
      if (meta.special && max - min <= LIST_RANGE) {
        const options = Array.from({ length: max - min + 1 }, (_, i) => {
          const n = min + i;
          const special = meta.special?.[String(n)];
          return special
            ? { value: special, label: valueLabel(meta.id, special) }
            : { value: String(n), label: numberLabel(meta, n) };
        }).filter(
          (o) =>
            meta.id !== "system.startupProfile" ||
            o.value === value ||
            !/^\d+$/.test(o.value) ||
            profileName(Number(o.value)) !== "",
        );
        return (
          <select
            aria-label={label}
            value={value}
            onChange={(e) => void changeSetting(meta.id, e.currentTarget.value)}
          >
            {options.map((o) => (
              <option key={o.value} value={o.value}>
                {o.label}
              </option>
            ))}
          </select>
        );
      }
      // Integers: the number follows the slider while dragging; the board hears the release. A
      // special number (the left end: Auto) is shown and sent by its name.
      const named = (raw: string) => meta.special?.[raw] ?? raw;
      const shown = named(dragging ?? value);
      return (
        <span class="range">
          <input
            type="range"
            aria-label={label}
            min={meta.min}
            max={meta.max}
            step={1}
            value={numberOf(meta, shown)}
            onInput={(e) => setDragging(e.currentTarget.value)}
            onChange={(e) => {
              setDragging(null);
              void changeSetting(meta.id, named(e.currentTarget.value));
            }}
          />
          <output>{valueLabel(meta.id, shown)}</output>
        </span>
      );
    }
  }
}

/** Live feedback for the sampling settings: how much of the last second the line PLL held. */
function SyncQuality() {
  const q = status.value?.source.syncQuality;
  const known = typeof q === "number";
  const tone = !known ? "" : q >= 98 ? "good" : q >= 80 ? "warn" : "bad";
  return (
    <li class="setting">
      <div class="setting-head">
        <span class="setting-label">{t("input.syncQuality")}</span>
        {known ? (
          <span class={`sync-quality ${tone}`}>
            <meter min={0} max={100} low={80} high={98} optimum={100} value={q} />
            {` ${q} %`}
          </span>
        ) : (
          <span class="muted">{t("input.syncQuality.none")}</span>
        )}
      </div>
      <p class="setting-help">{t("input.syncQuality.help")}</p>
    </li>
  );
}

const LEVELS_NOTICE: Readonly<Record<string, () => Notice>> = {
  running: () => info(t("action.calibrate.started")),
  done: () => info(t("action.calibrate.done")),
  notWhite: () => bad(t("action.calibrate.notWhite")),
  tooHot: () => bad(t("action.calibrate.tooHot")),
  aborted: () => bad(t("action.calibrate.aborted")),
};

// A refused calibration can end between two polls (bench: under 1.5 s), so the page may never see
// it running. Past this, it reports whatever the board says last.
const CALIBRATE_SEEN_MS = 5000;

/**
 * "Calibrate levels", then the board's verdict. The status keeps the last outcome, so after a press
 * the page waits for this run: seen running, an outcome other than the one before the press, or a
 * few seconds.
 */
export function InputActions() {
  const [phase, setPhase] = useState<"idle" | "asked" | "running" | "finished">("idle");
  const [before, setBefore] = useState<string>("none");
  const [refusal, setRefusal] = useState<Notice | null>(null);
  const outcome = status.value?.levels ?? "none";
  useEffect(() => {
    if (phase === "asked" && outcome === "running") setPhase("running");
    else if (phase === "asked" && outcome !== before) setPhase("finished");
    if (phase === "running" && outcome !== "running") setPhase("finished");
  }, [phase, outcome, before]);
  useEffect(() => {
    if (phase !== "asked") return;
    const timer = setTimeout(() => setPhase("finished"), CALIBRATE_SEEN_MS);
    return () => clearTimeout(timer);
  }, [phase]);
  const busy = phase === "asked" || phase === "running";
  const notice =
    refusal ??
    (busy
      ? info(t("action.calibrate.started"))
      : phase === "finished"
        ? (LEVELS_NOTICE[outcome]?.() ?? null)
        : null);
  return (
    <li class="setting">
      <div class="setting-head">
        <span class="setting-label">{t("action.calibrate")}</span>
        <button
          type="button"
          disabled={busy}
          onClick={async () => {
            setRefusal(null);
            setBefore(outcome);
            const r = await runCommand("calibrate");
            if (r.ok) {
              setPhase("asked");
            } else {
              setRefusal(
                bad(
                  t(
                    r.reason === "noStablePicture" ? "action.calibrate.noPicture" : "action.failed",
                  ),
                ),
              );
            }
          }}
        >
          {t("action.calibrate")}
        </button>
      </div>
      <p class="setting-help">{t("action.calibrate.help")}</p>
      <Note notice={notice} />
    </li>
  );
}

/** Restart and factory reset: each asks for a second press, as the board's own menu does. */
function SystemActions() {
  const [armed, setArmed] = useState<"reboot" | "factoryReset" | null>(null);
  const [note, setNote] = useState<Notice | null>(null);
  const [busy, setBusy] = useState(false);
  const press = async (what: "reboot" | "factoryReset") => {
    if (armed !== what) {
      setArmed(what);
      setNote(warn(t(what === "reboot" ? "action.reboot.confirm" : "action.factoryReset.confirm")));
      return;
    }
    setArmed(null);
    setBusy(true);
    const ok = (await runCommand(what)).ok;
    setNote(ok ? info(t("action.done")) : bad(t("action.failed")));
    setBusy(false);
  };
  return (
    <li class="setting">
      <div class="actions">
        <button type="button" disabled={busy} onClick={() => void press("reboot")}>
          {t("action.reboot")}
        </button>
        <button type="button" disabled={busy} onClick={() => void press("factoryReset")}>
          {t("action.factoryReset")}
        </button>
        {armed === "factoryReset" && (
          <button
            type="button"
            onClick={async () => {
              try {
                downloadJson("gbs-backup.json", await fetchBackup());
              } catch {
                setNote(bad(t("action.failed")));
              }
            }}
          >
            {t("profiles.backup.download")}
          </button>
        )}
      </div>
      <Note notice={note} />
    </li>
  );
}

/** One setting: its control, help, why it has no effect now, and a way back to the default. */
function SettingRow({ meta }: { meta: SettingMeta }) {
  const text = settingText(meta.id);
  const state = fieldState.value[meta.id] ?? "saved";
  const value = values.value[meta.id];
  const idle = idleReason(meta.id, status.value, values.value);
  const fallback = defaultText(meta);
  const helpId = `help-${meta.id}`;
  return (
    <li class={`setting setting-${state}${idle ? " setting-idle" : ""}`}>
      <div class="setting-head">
        <span class="setting-label">{text.label}</span>
        <span class="setting-control" aria-describedby={helpId}>
          <Control meta={meta} />
          {value !== undefined && value !== fallback && (
            <button
              type="button"
              class="reset"
              title={t("settings.resetTo", { value: valueLabel(meta.id, fallback) })}
              onClick={() => void changeSetting(meta.id, fallback)}
            >
              {t("settings.reset")}
            </button>
          )}
        </span>
      </div>
      <p class="setting-help" id={helpId}>
        {text.help}
      </p>
      {idle && <Note notice={info(t(idle))} />}
      {state === "pending" && <Note notice={info(t("settings.saving"))} />}
      {state === "failed" && <Note notice={bad(t("settings.failed"))} />}
    </li>
  );
}

export function SettingsPage({ group }: { group: SettingGroup }) {
  const visible = SETTINGS.filter(
    (m) =>
      m.group === group &&
      m.available &&
      !ELSEWHERE.has(m.id) &&
      (showAdvanced.value || m.tier === "essential"),
  );
  if (!loaded.value) {
    return <p class="muted">{t("conn.reconnecting")}</p>;
  }
  const sections = SECTIONS[group] ?? [];
  const inSection = new Set(sections.flatMap((x) => x.ids));
  const loose = visible.filter((m) => !inSection.has(m.id));
  return (
    <section>
      <ul class="settings">
        {group === "input" && showAdvanced.value && <SyncQuality />}
        {loose.map((meta) => (
          <SettingRow key={meta.id} meta={meta} />
        ))}
        {group === "input" && showAdvanced.value && <InputActions />}
      </ul>
      {sections.map((section) => {
        const rows = section.ids
          .map((id) => visible.find((m) => m.id === id))
          .filter((m): m is SettingMeta => m !== undefined);
        if (rows.length === 0) return null;
        return (
          <div key={section.title} class="settings-section">
            <h2 class="section-title">{t(section.title)}</h2>
            <ul class="settings">
              {rows.map((meta) => (
                <SettingRow key={meta.id} meta={meta} />
              ))}
            </ul>
          </div>
        );
      })}
      {group === "system" && (
        <div class="settings-section">
          <h2 class="section-title">{t("section.maintenance")}</h2>
          <ul class="settings">
            <Backup />
            <FirmwareUpdate />
            <SystemActions />
          </ul>
        </div>
      )}
      <label class="toggle">
        <input
          type="checkbox"
          checked={showAdvanced.value}
          onChange={(e) => void setShowAdvanced(e.currentTarget.checked)}
        />
        {t("settings.advanced")}
      </label>
    </section>
  );
}
