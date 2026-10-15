import { useEffect, useState } from "preact/hooks";
import { exportProfile, fetchBackup } from "../api/endpoints";
import type { ProfileSummary } from "../api/types";
import { downloadJson } from "../download";
import { SETTINGS } from "../gen/settings.gen";
import type { MessageKey } from "../i18n";
import { settingText, t } from "../i18n";
import {
  describeSource,
  firstFreeSlot,
  loadProfiles,
  parseBackupFile,
  parseProfileFile,
  profiles,
  reasonKey,
  restoreBackup,
  runProfile,
  SLOTS,
} from "../state/profiles";
import { status } from "../state/status";
import { Control } from "./settings-page";

const STARTUP = SETTINGS.find((m) => m.id === "system.startupProfile");

/** system.startupProfile, here too: it names the profiles. */
function StartupProfile() {
  if (!STARTUP) return null;
  const text = settingText(STARTUP.id);
  return (
    <div class="setting">
      <div class="setting-head">
        <span class="setting-label">{text.label}</span>
        <Control meta={STARTUP} />
      </div>
      <p class="setting-help">{text.help}</p>
    </div>
  );
}

function fileSafe(name: string): string {
  return name.replace(/[^\w.-]+/g, "-").replace(/^-+|-+$/g, "") || "profile";
}

/** A message under a row: the board's answer to the last thing done there. */
function Note({ note }: { note: string | null }) {
  return note ? <p class="setting-note">{note}</p> : null;
}

function ProfileRow({ p }: { p: ProfileSummary }) {
  const list = profiles.value;
  const active = list?.active === p.slot;
  const [busy, setBusy] = useState(false);
  const [note, setNote] = useState<string | null>(null);
  const [armed, setArmed] = useState<"save" | "delete" | null>(null);
  const [renaming, setRenaming] = useState<string | null>(null);

  const act = async (run: () => Promise<string | null>) => {
    setBusy(true);
    setNote(await run());
    setBusy(false);
  };
  const confirmFirst = (what: "save" | "delete", run: () => Promise<string | null>) => {
    if (armed !== what) {
      setArmed(what);
      setNote(t("profiles.confirm"));
      return;
    }
    setArmed(null);
    void act(run);
  };

  return (
    <li class={`setting profile${active ? " profile-active" : ""}`}>
      <div class="setting-head">
        <span class="setting-label">
          {p.slot} · {p.name}
          {active && list?.modified && <span class="muted"> — {t("profiles.modified")}</span>}
        </span>
        <button
          type="button"
          class="primary"
          disabled={busy}
          onClick={() =>
            act(async () => {
              const a = await runProfile({ action: "load", slot: p.slot });
              return a.ok ? t("profiles.loaded") : t(reasonKey(a));
            })
          }
        >
          {t("profiles.load")}
        </button>
      </div>
      <p class="setting-help">
        {p.source
          ? t("profiles.loadsFor", { source: describeSource(p.source) })
          : t("profiles.manual")}
      </p>
      {renaming !== null ? (
        <form
          class="actions"
          onSubmit={(e) => {
            e.preventDefault();
            void act(async () => {
              const a = await runProfile({ action: "rename", slot: p.slot, name: renaming });
              setRenaming(null);
              return a.ok ? null : t(reasonKey(a));
            });
          }}
        >
          <input
            aria-label={t("profiles.name")}
            value={renaming}
            maxLength={23}
            onInput={(e) => setRenaming(e.currentTarget.value)}
          />
          <button type="submit" disabled={busy || renaming.trim() === ""}>
            {t("profiles.save")}
          </button>
        </form>
      ) : (
        <div class="actions">
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              confirmFirst("save", async () => {
                const a = await runProfile({ action: "save", slot: p.slot, source: "keep" });
                return a.ok ? t("profiles.saved") : t(reasonKey(a));
              })
            }
          >
            {t("profiles.saveHere")}
          </button>
          <button type="button" disabled={busy} onClick={() => setRenaming(p.name)}>
            {t("profiles.rename")}
          </button>
          <button
            type="button"
            disabled={busy || (!p.source && !list?.source)}
            onClick={() =>
              act(async () => {
                const a = await runProfile({ action: "source", slot: p.slot, on: !p.source });
                return a.ok ? null : t(reasonKey(a));
              })
            }
          >
            {p.source ? t("profiles.autoOff") : t("profiles.autoOn")}
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              act(async () => {
                try {
                  const doc = await exportProfile(p.slot);
                  downloadJson(`gbs-profile-${p.slot}-${fileSafe(doc.name)}.json`, doc);
                  return null;
                } catch {
                  return t("action.failed");
                }
              })
            }
          >
            {t("profiles.export")}
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              confirmFirst("delete", async () => {
                const a = await runProfile({ action: "delete", slot: p.slot });
                return a.ok ? null : t(reasonKey(a));
              })
            }
          >
            {t("profiles.delete")}
          </button>
        </div>
      )}
      <Note note={note} />
    </li>
  );
}

/** Save the current settings into a slot, or import a profile file into it. */
function SaveForm() {
  const list = profiles.value;
  const [slot, setSlot] = useState(0);
  const [name, setName] = useState("");
  const [forSource, setForSource] = useState(false);
  const [note, setNote] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const chosen = slot || firstFreeSlot() || 1;
  const existing = list?.profiles.find((p) => p.slot === chosen);

  // A slot that loads by itself keeps doing so unless the box is cleared.
  useEffect(() => {
    setForSource(Boolean(existing?.source));
  }, [chosen, existing?.source]);

  if (!list) return null;
  const full = firstFreeSlot() === 0;

  const save = async (e: Event) => {
    e.preventDefault();
    setBusy(true);
    const a = await runProfile({
      action: "save",
      slot: chosen,
      name: name.trim(),
      source: forSource ? "this" : "none",
    });
    setNote(a.ok ? t("profiles.saved") : t(reasonKey(a)));
    if (a.ok) setName("");
    setBusy(false);
  };

  const importFile = async (file: File) => {
    setBusy(true);
    const content = parseProfileFile(await file.text());
    if (!content) {
      setNote(t("profiles.notProfile"));
    } else {
      const a = await runProfile({ action: "import", slot: chosen, profile: content });
      setNote(
        !a.ok
          ? t(reasonKey(a))
          : a.skipped?.length
            ? t("profiles.importedSkipped", { ids: a.skipped.join(", ") })
            : t("profiles.imported"),
      );
    }
    setBusy(false);
  };

  return (
    <div class="setting">
      <h2 class="setting-label">{t("profiles.saveTitle")}</h2>
      {full && <p class="setting-help">{t("profiles.full")}</p>}
      <form class="join" onSubmit={save}>
        <label>
          {t("profiles.slot")}
          <select
            value={chosen}
            onChange={(e) => setSlot(Number(e.currentTarget.value))}
            aria-label={t("profiles.slot")}
          >
            {Array.from({ length: SLOTS }, (_, i) => i + 1).map((n) => {
              const used = list.profiles.find((p) => p.slot === n);
              return (
                <option key={n} value={n}>
                  {used ? `${n} · ${used.name}` : t("profiles.slotFree", { slot: n })}
                </option>
              );
            })}
          </select>
        </label>
        <label>
          {t("profiles.name")}
          <input
            value={name}
            maxLength={23}
            placeholder={existing?.name ?? t("oled.profiles.slot", { n: chosen })}
            onInput={(e) => setName(e.currentTarget.value)}
          />
        </label>
        <label class="toggle">
          <input
            type="checkbox"
            checked={forSource}
            disabled={!list.source}
            onChange={(e) => setForSource(e.currentTarget.checked)}
          />
          {t("profiles.forSource")}
        </label>
        <button type="submit" class="primary" disabled={busy}>
          {t("profiles.save")}
        </button>
        <label>
          {t("profiles.import")}
          <input
            type="file"
            accept="application/json,.json"
            disabled={busy}
            onChange={(e) => {
              const file = e.currentTarget.files?.[0];
              e.currentTarget.value = "";
              if (file) void importFile(file);
            }}
          />
        </label>
      </form>
      <Note note={note} />
    </div>
  );
}

function Backup() {
  const [note, setNote] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const restore = async (file: File) => {
    setBusy(true);
    const backup = parseBackupFile(await file.text());
    if (!backup) {
      setNote(t("profiles.backup.notBackup"));
    } else {
      const count = await restoreBackup(backup);
      setNote(count === null ? t("action.failed") : t("profiles.backup.restored", { count }));
    }
    setBusy(false);
  };
  return (
    <div class="setting">
      <h2 class="setting-label">{t("profiles.backup")}</h2>
      <p class="setting-help">{t("profiles.backup.help")}</p>
      <div class="actions">
        <button
          type="button"
          disabled={busy}
          onClick={async () => {
            try {
              const backup = await fetchBackup();
              downloadJson("gbs-backup.json", backup);
            } catch {
              setNote(t("action.failed"));
            }
          }}
        >
          {t("profiles.backup.download")}
        </button>
        <label class="file">
          {t("profiles.backup.restore")}
          <input
            type="file"
            accept="application/json,.json"
            disabled={busy}
            onChange={(e) => {
              const file = e.currentTarget.files?.[0];
              e.currentTarget.value = "";
              if (file) void restore(file);
            }}
          />
        </label>
      </div>
      <Note note={note} />
    </div>
  );
}

export function ProfilesPage() {
  useEffect(() => {
    void loadProfiles();
  }, []);
  const list = profiles.value;
  if (!list) {
    return <p class="muted">{t("conn.reconnecting")}</p>;
  }
  const inUse = list.profiles.find((p) => p.slot === list.active);
  const safe = status.value?.safeMode === true;
  const current: { key: MessageKey; params?: Record<string, string> } = inUse
    ? { key: "profiles.inUse", params: { name: `${inUse.slot} · ${inUse.name}` } }
    : { key: "profiles.noneInUse" };
  return (
    <section>
      <p class="headline">
        {t(current.key, current.params)}
        {inUse && list.modified && <span class="muted"> — {t("profiles.modified")}</span>}
      </p>
      <p class="muted">
        {list.source
          ? t("profiles.sourceNow", { source: describeSource(list.source) })
          : t("profiles.noSource")}
      </p>
      {safe && <p class="banner banner-bad">{t("profiles.reason.safeMode")}</p>}
      <p class="setting-help">{t("profiles.help")}</p>
      {list.profiles.length === 0 ? (
        <p class="muted">{t("profiles.empty")}</p>
      ) : (
        <ul class="settings">
          {list.profiles.map((p) => (
            <ProfileRow key={p.slot} p={p} />
          ))}
        </ul>
      )}
      <SaveForm />
      <StartupProfile />
      <Backup />
    </section>
  );
}
