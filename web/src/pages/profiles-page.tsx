import { useEffect, useState } from "preact/hooks";
import { exportProfile, fetchBackup } from "../api/endpoints";
import type { BackupDocument, ProfileContent, ProfileSummary } from "../api/types";
import { bad, info, Note, type Notice, warn } from "../components/note";
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

function ProfileRow({ p }: { p: ProfileSummary }) {
  const list = profiles.value;
  const active = list?.active === p.slot;
  const [busy, setBusy] = useState(false);
  const [note, setNote] = useState<Notice | null>(null);
  const [armed, setArmed] = useState<"save" | "delete" | null>(null);
  const [renaming, setRenaming] = useState<string | null>(null);

  const act = async (run: () => Promise<Notice | null>) => {
    setBusy(true);
    setNote(await run());
    setBusy(false);
  };
  const confirmFirst = (what: "save" | "delete", run: () => Promise<Notice | null>) => {
    if (armed !== what) {
      setArmed(what);
      setNote(warn(t("profiles.confirm")));
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
              return a.ok ? info(t("profiles.loaded")) : bad(t(reasonKey(a)));
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
              return a.ok ? null : bad(t(reasonKey(a)));
            });
          }}
        >
          <input
            aria-label={t("profiles.name")}
            value={renaming}
            maxLength={23}
            ref={(el) => el?.focus()}
            onInput={(e) => setRenaming(e.currentTarget.value)}
            onKeyDown={(e) => {
              if (e.key === "Escape") setRenaming(null);
            }}
          />
          <button type="submit" disabled={busy || renaming.trim() === ""}>
            {t("profiles.rename")}
          </button>
          <button type="button" onClick={() => setRenaming(null)}>
            {t("common.cancel")}
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
                return a.ok ? info(t("profiles.saved")) : bad(t(reasonKey(a)));
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
                return a.ok ? null : bad(t(reasonKey(a)));
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
                  return bad(t("action.failed"));
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
                return a.ok ? null : bad(t(reasonKey(a)));
              })
            }
          >
            {t("profiles.delete")}
          </button>
        </div>
      )}
      <Note notice={note} />
    </li>
  );
}

/** Save the current settings into a slot, or import a profile file into it. */
function SaveForm() {
  const list = profiles.value;
  const [slot, setSlot] = useState(0);
  const [name, setName] = useState("");
  const [forSource, setForSource] = useState(false);
  const [note, setNote] = useState<Notice | null>(null);
  const [busy, setBusy] = useState(false);
  // With every slot used, nothing is preselected: replacing one must be a choice.
  const chosen = slot || firstFreeSlot();
  const existing = list?.profiles.find((p) => p.slot === chosen);
  const [armed, setArmed] = useState<"save" | "import" | null>(null);
  const [pendingImport, setPendingImport] = useState<ProfileContent | null>(null);

  // A slot that loads by itself keeps doing so unless the box is cleared.
  useEffect(() => {
    setForSource(Boolean(existing?.source));
  }, [chosen, existing?.source]);

  if (!list) return null;
  const full = firstFreeSlot() === 0;

  const replacing = () =>
    warn(t("profiles.replaceConfirm", { name: `${chosen} · ${existing?.name ?? ""}` }));

  const save = async (e: Event) => {
    e.preventDefault();
    if (existing && armed !== "save") {
      setArmed("save");
      setNote(replacing());
      return;
    }
    setArmed(null);
    setBusy(true);
    // Without a source now, a slot that loads by itself keeps the source it has.
    const source = forSource ? (list?.source ? "this" : "keep") : "none";
    const a = await runProfile({ action: "save", slot: chosen, name: name.trim(), source });
    setNote(a.ok ? info(t("profiles.saved")) : bad(t(reasonKey(a))));
    if (a.ok) setName("");
    setBusy(false);
  };

  const runImport = async (content: ProfileContent) => {
    setArmed(null);
    setPendingImport(null);
    setBusy(true);
    const a = await runProfile({ action: "import", slot: chosen, profile: content });
    setNote(
      !a.ok
        ? bad(t(reasonKey(a)))
        : a.skipped?.length
          ? warn(t("profiles.importedSkipped", { ids: a.skipped.join(", ") }))
          : info(t("profiles.imported")),
    );
    setBusy(false);
  };

  const importFile = async (file: File) => {
    const content = parseProfileFile(await file.text());
    if (!content) {
      setNote(bad(t("profiles.notProfile")));
    } else if (existing) {
      setPendingImport(content);
      setArmed("import");
      setNote(replacing());
    } else {
      await runImport(content);
    }
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
            onChange={(e) => {
              setSlot(Number(e.currentTarget.value));
              setArmed(null);
              setPendingImport(null);
              setNote(null);
            }}
            aria-label={t("profiles.slot")}
          >
            {chosen === 0 && (
              <option value={0} disabled>
                {t("profiles.chooseSlot")}
              </option>
            )}
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
        <button type="submit" class="primary" disabled={busy || chosen === 0}>
          {armed === "save" ? t("profiles.replace") : t("profiles.save")}
        </button>
        <label>
          {t("profiles.import")}
          <input
            type="file"
            accept="application/json,.json"
            disabled={busy || chosen === 0}
            onChange={(e) => {
              const file = e.currentTarget.files?.[0];
              e.currentTarget.value = "";
              if (file) void importFile(file);
            }}
          />
        </label>
        {pendingImport && (
          <button type="button" class="primary" onClick={() => void runImport(pendingImport)}>
            {t("profiles.replace")}
          </button>
        )}
      </form>
      <Note notice={note} />
    </div>
  );
}

/** Download a backup; restore one after a second press, saying what did not come back. */
function Backup() {
  const [note, setNote] = useState<Notice | null>(null);
  const [busy, setBusy] = useState(false);
  const [pending, setPending] = useState<BackupDocument | null>(null);
  const choose = async (file: File) => {
    const backup = parseBackupFile(await file.text());
    if (!backup) {
      setPending(null);
      setNote(bad(t("profiles.backup.notBackup")));
      return;
    }
    setPending(backup);
    setNote(warn(t("profiles.backup.confirm", { count: Object.keys(backup.profiles).length })));
  };
  const restore = async (backup: BackupDocument) => {
    setPending(null);
    setBusy(true);
    const report = await restoreBackup(backup);
    if (!report) {
      setNote(bad(t("action.failed")));
    } else {
      const parts = [t("profiles.backup.restored", { count: report.profiles })];
      if (report.refused.length) {
        parts.push(t("profiles.backup.refused", { ids: report.refused.join(", ") }));
      }
      if (report.revertArmed) parts.push(t("profiles.backup.keep"));
      const text = parts.join(" ");
      setNote(report.refused.length || report.revertArmed ? warn(text) : info(text));
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
              setNote(bad(t("action.failed")));
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
              if (file) void choose(file);
            }}
          />
        </label>
        {pending && (
          <button type="button" class="primary" onClick={() => void restore(pending)}>
            {t("profiles.backup.restoreNow")}
          </button>
        )}
      </div>
      <Note notice={note} />
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
      {safe && <p class="setting-note bad">{t("profiles.reason.safeMode")}</p>}
      <p class="setting-help">{t("profiles.help")}</p>
      <fieldset class="plain" disabled={safe}>
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
      </fieldset>
    </section>
  );
}
