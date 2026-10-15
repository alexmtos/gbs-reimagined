import { useState } from "preact/hooks";
import { fetchBackup, runCommand } from "../api/endpoints";
import { FirmwareUpdate } from "../components/firmware-update";
import { downloadJson } from "../download";
import type { SettingGroup, SettingMeta } from "../gen/settings.gen";
import { SETTINGS } from "../gen/settings.gen";
import type { MessageKey } from "../i18n";
import { settingText, t, valueLabel } from "../i18n";
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
  return String(n);
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
          {(meta.values ?? []).map((v) => (
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
        });
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
      // Integers: the number follows the slider while dragging; the board hears the release.
      const shown = dragging ?? value;
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
              void changeSetting(meta.id, e.currentTarget.value);
            }}
          />
          <output>{valueLabel(meta.id, shown)}</output>
        </span>
      );
    }
  }
}

function ActionButton({ label, run }: { label: string; run: () => Promise<MessageKey | null> }) {
  const [busy, setBusy] = useState(false);
  const [note, setNote] = useState<MessageKey | null>(null);
  return (
    <span class="action">
      <button
        type="button"
        disabled={busy}
        onClick={async () => {
          setBusy(true);
          setNote(await run());
          setBusy(false);
        }}
      >
        {label}
      </button>
      {note && <span class="muted">{t(note)}</span>}
    </span>
  );
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

function InputActions() {
  return (
    <li class="setting">
      <div class="setting-head">
        <span class="setting-label">{t("action.calibrate")}</span>
        <ActionButton
          label={t("action.calibrate")}
          run={async () => {
            const r = await runCommand("calibrate");
            if (r.ok) return "action.calibrate.started";
            return r.reason === "noStablePicture" ? "action.calibrate.noPicture" : "action.failed";
          }}
        />
      </div>
      <p class="setting-help">{t("action.calibrate.help")}</p>
    </li>
  );
}

function SystemActions() {
  const [armed, setArmed] = useState(false);
  return (
    <li class="setting">
      <div class="actions">
        <ActionButton
          label={t("action.reboot")}
          run={async () => ((await runCommand("reboot")).ok ? "action.done" : "action.failed")}
        />
        <ActionButton
          label={t("action.factoryReset")}
          run={async () => {
            if (!armed) {
              setArmed(true);
              return "action.factoryReset.confirm";
            }
            setArmed(false);
            return (await runCommand("factoryReset")).ok ? "action.done" : "action.failed";
          }}
        />
        {armed && (
          <ActionButton
            label={t("profiles.backup.download")}
            run={async () => {
              try {
                downloadJson("gbs-backup.json", await fetchBackup());
                return null;
              } catch {
                return "action.failed";
              }
            }}
          />
        )}
      </div>
    </li>
  );
}

// Chosen elsewhere on the page: the language in the header.
const ELSEWHERE: ReadonlySet<string> = new Set(["system.language"]);

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
  return (
    <section>
      <ul class="settings">
        {group === "input" && showAdvanced.value && <SyncQuality />}
        {visible.map((meta) => {
          const text = settingText(meta.id);
          const state = fieldState.value[meta.id] ?? "saved";
          return (
            <li key={meta.id} class={`setting setting-${state}`}>
              <div class="setting-head">
                <span class="setting-label">{text.label}</span>
                <Control meta={meta} />
              </div>
              <p class="setting-help">{text.help}</p>
              {state === "pending" && <p class="setting-note">{t("settings.saving")}</p>}
              {state === "failed" && <p class="setting-note bad">{t("settings.failed")}</p>}
            </li>
          );
        })}
        {group === "input" && showAdvanced.value && <InputActions />}
        {group === "system" && <FirmwareUpdate />}
        {group === "system" && <SystemActions />}
      </ul>
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
