import { signal } from "@preact/signals";
import type { SettingGroup, SettingMeta } from "../gen/settings.gen";
import { SETTINGS } from "../gen/settings.gen";
import { settingText, t, valueLabel } from "../i18n";

// M1 delivers editing through /api/settings; until then the page lists what the schema defines,
// so the generated metadata and translations are exercised end to end.
export const showAdvanced = signal(false);
const CURRENT_MILESTONE = 1;

function describeDefault(meta: SettingMeta): string {
  if (meta.type === "bool") return meta.default ? t("common.on") : t("common.off");
  if (meta.type === "enum") return valueLabel(meta.id, String(meta.default));
  const special = meta.special?.[String(meta.default)];
  return special ? valueLabel(meta.id, special) : String(meta.default);
}

export function SettingsPage({ group }: { group: SettingGroup }) {
  const visible = SETTINGS.filter(
    (m) =>
      m.group === group &&
      m.milestone <= CURRENT_MILESTONE &&
      (showAdvanced.value || m.tier === "essential"),
  );
  return (
    <section>
      <ul class="settings">
        {visible.map((meta) => {
          const text = settingText(meta.id);
          return (
            <li key={meta.id} class="setting">
              <div class="setting-head">
                <span class="setting-label">{text.label}</span>
                <span class="setting-value">{describeDefault(meta)}</span>
              </div>
              <p class="setting-help">{text.help}</p>
            </li>
          );
        })}
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
