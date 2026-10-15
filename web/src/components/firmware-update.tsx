import { useState } from "preact/hooks";
import type { MessageKey } from "../i18n";
import { t } from "../i18n";
import { status } from "../state/status";
import {
  ChecksumError,
  downloadOffer,
  fetchLatest,
  type Offer,
  pickOffer,
  type UpdateAnswer,
  uploadFirmware,
} from "../update/update";
import { bad, info, Note, type Notice } from "./note";

function reasonKey(answer: UpdateAnswer): MessageKey {
  switch (answer.reason) {
    case "notGzip":
      return "update.reason.notGzip";
    case "wrongBoard":
      return "update.reason.wrongBoard";
    case "noBoardId":
      return "update.reason.noBoardId";
    default:
      return "update.reason.failed";
  }
}

/** System page: the installed version, an update file, and "check for updates" (ADR 0012). */
export function FirmwareUpdate() {
  const version = status.value?.board.version ?? "";
  const [file, setFile] = useState<File | null>(null);
  const [busy, setBusy] = useState(false);
  const [progress, setProgress] = useState<number | null>(null);
  const [note, setNote] = useState<Notice | null>(null);
  const [offer, setOffer] = useState<Offer | null>(null);
  const [beta, setBeta] = useState(false);
  const [installed, setInstalled] = useState<string | null>(null);

  const send = async (blob: Blob, name: string) => {
    setBusy(true);
    setProgress(0);
    const answer = await uploadFirmware(blob, name, setProgress);
    setProgress(null);
    if (answer.ok) {
      setInstalled(answer.version ?? "");
      setNote(info(t("update.done", { version: answer.version ?? "" })));
      setOffer(null);
    } else {
      setNote(bad(t(reasonKey(answer))));
    }
    setBusy(false);
  };

  const check = async () => {
    setBusy(true);
    setNote(info(t("update.checking")));
    setOffer(null);
    try {
      const found = pickOffer(await fetchLatest(), version, beta);
      setOffer(found);
      setNote(
        info(found ? t("update.available", { version: found.version }) : t("update.upToDate")),
      );
    } catch {
      setNote(bad(t("update.offline")));
    }
    setBusy(false);
  };

  const installOffer = async () => {
    if (!offer) return;
    setBusy(true);
    try {
      const blob = await downloadOffer(offer);
      await send(blob, offer.file.ota.split("/").pop() ?? "update.ota.bin.gz");
    } catch (e) {
      setNote(bad(e instanceof ChecksumError ? t("update.badChecksum") : t("update.offline")));
      setBusy(false);
    }
  };

  const running = installed !== null && version === installed;
  return (
    <li class="setting">
      <div class="setting-head">
        <span class="setting-label">{t("update.title")}</span>
        <span class="muted">{t("update.installed", { version })}</span>
      </div>
      <p class="setting-help">{t("update.help")}</p>
      <div class="actions">
        <button type="button" disabled={busy} onClick={check}>
          {t("update.check")}
        </button>
        <label class="toggle">
          <input
            type="checkbox"
            checked={beta}
            onChange={(e) => setBeta(e.currentTarget.checked)}
          />
          {t("update.beta")}
        </label>
      </div>
      {offer && (
        <div class="actions">
          <button type="button" class="primary" disabled={busy} onClick={installOffer}>
            {t("update.installOffer")}
          </button>
          {offer.notes && (
            <a href={offer.notes} target="_blank" rel="noreferrer">
              {t("update.notes")}
            </a>
          )}
        </div>
      )}
      <label class="file">
        {t("update.file")}
        <input
          type="file"
          accept=".gz,application/gzip"
          disabled={busy}
          onChange={(e) => setFile(e.currentTarget.files?.[0] ?? null)}
        />
      </label>
      <div class="actions">
        <button
          type="button"
          disabled={busy || !file}
          onClick={() => file && void send(file, file.name)}
        >
          {t("update.install")}
        </button>
        {progress !== null && (
          <span class="muted">
            <progress max={1} value={progress} />{" "}
            {t("update.sending", { percent: Math.round(progress * 100) })}
          </span>
        )}
      </div>
      <Note notice={running ? info(t("update.running", { version })) : note} />
    </li>
  );
}
