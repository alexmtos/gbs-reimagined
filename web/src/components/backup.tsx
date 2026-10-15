import { useState } from "preact/hooks";
import { fetchBackup } from "../api/endpoints";
import type { BackupDocument } from "../api/types";
import { downloadJson } from "../download";
import { t } from "../i18n";
import { parseBackupFile, restoreBackup } from "../state/profiles";
import { bad, info, Note, type Notice, warn } from "./note";

/** Download a backup; restore one after a second press, saying what did not come back. */
export function Backup() {
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
    <li class="setting">
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
    </li>
  );
}
