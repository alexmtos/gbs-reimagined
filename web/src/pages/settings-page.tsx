import { signal } from "@preact/signals";
import { useState } from "preact/hooks";
import { runCommand } from "../api/endpoints";
import type { SettingGroup, SettingMeta } from "../gen/settings.gen";
import { SETTINGS } from "../gen/settings.gen";
import type { MessageKey } from "../i18n";
import { settingText, t, valueLabel } from "../i18n";
import { changeSetting, fieldState, loaded, values } from "../state/settings";

export const showAdvanced = signal(false);
const CURRENT_MILESTONE = 1;

/** The control for one setting; the board's answer decides what it finally shows. */
function Control({ meta }: { meta: SettingMeta }) {
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
            value={Number(shown)}
            onInput={(e) => setDragging(e.currentTarget.value)}
            onChange={(e) => {
              setDragging(null);
              void changeSetting(meta.id, e.currentTarget.value);
            }}
          />
          <output>{shown}</output>
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
      </div>
    </li>
  );
}

export function SettingsPage({ group }: { group: SettingGroup }) {
  const visible = SETTINGS.filter(
    (m) =>
      m.group === group &&
      m.milestone <= CURRENT_MILESTONE &&
      (showAdvanced.value || m.tier === "essential"),
  );
  if (!loaded.value) {
    return <p class="muted">{t("conn.reconnecting")}</p>;
  }
  return (
    <section>
      <ul class="settings">
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
        {group === "system" && <SystemActions />}
      </ul>
      <label class="toggle">
        <input
          type="checkbox"
          checked={showAdvanced.value}
          onChange={(e) => {
            showAdvanced.value = e.currentTarget.checked;
          }}
        />
        {t("settings.advanced")}
      </label>
    </section>
  );
}
