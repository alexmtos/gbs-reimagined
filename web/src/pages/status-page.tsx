import type { Status } from "../api/types";
import type { MessageKey } from "../i18n";
import { formatNumber, t, valueLabel } from "../i18n";
import { values } from "../state/settings";
import { status } from "../state/status";

/**
 * The headline sentence. With no signal the board reports no port: the page names the input the
 * user chose instead, or none when the board watches all of them.
 */
export function headline(s: Status, inputSetting: string | undefined): string {
  switch (s.headline) {
    case "noSignal": {
      const port = s.source.port !== "none" ? s.source.port : inputSetting;
      return !port || port === "auto"
        ? t("headline.noSignalAuto")
        : t("headline.noSignal", { input: valueLabel("input.port", port) });
    }
    case "scaling":
      return t("headline.scaling", {
        source: s.source.standard || t("common.unknown"),
        output: valueLabel("output.resolution", s.output.mode),
      });
    default:
      return t(`headline.${s.headline}` as MessageKey);
  }
}

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div class="row">
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  );
}

/** "1 h 02 min", "3 min 20 s" or "45 s". */
export function uptimeText(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const sec = seconds % 60;
  if (h > 0) return t("status.uptimeHours", { h, m: String(m).padStart(2, "0") });
  if (m > 0) return t("status.uptimeMinutes", { m, s: String(sec).padStart(2, "0") });
  return t("status.uptimeSeconds", { s: sec });
}

const SYNC_KEY: Readonly<Record<string, MessageKey>> = {
  hv: "sync.hv",
  csync: "sync.csync",
  sog: "sync.sog",
};

export function StatusPage() {
  const s = status.value;
  if (!s) {
    return <p class="muted">{t("conn.reconnecting")}</p>;
  }
  const lock =
    s.output.frameLock === "locked"
      ? t("common.on")
      : s.output.frameLock === "off"
        ? t("common.off")
        : t(
            s.output.frameLockReason
              ? (`status.lockReason.${s.output.frameLockReason}` as MessageKey)
              : "common.unknown",
          );
  // With no signal there is nothing to measure: a dash, not "0.00 kHz".
  const dash = t("common.unknown");
  const source = s.source.port !== "none";
  const sync = SYNC_KEY[s.source.sync];
  return (
    <section>
      <h1 class={`headline headline-${s.headline}`}>{headline(s, values.value["input.port"])}</h1>
      {s.profile && (
        <p class="muted">
          {t("status.profile")}: {s.profile.slot} · {s.profile.name}
          {s.profile.modified && ` — ${t("profiles.modified")}`}
        </p>
      )}
      <div class="cards">
        <dl class="card">
          <h2>{t("status.source")}</h2>
          <Row
            label={t("status.input")}
            value={source ? valueLabel("input.port", s.source.port) : dash}
          />
          <Row label={t("status.sync")} value={source && sync ? t(sync) : dash} />
          <Row label={t("status.standard")} value={s.source.standard || dash} />
          <Row label={t("status.lines")} value={s.source.lines ? String(s.source.lines) : dash} />
          <Row
            label={t("status.hfreq")}
            value={s.source.hFreqKHz ? `${formatNumber(s.source.hFreqKHz, 2)} kHz` : dash}
          />
          <Row
            label={t("status.vfreq")}
            value={s.source.vFreqHz ? `${formatNumber(s.source.vFreqHz, 3)} Hz` : dash}
          />
        </dl>
        <dl class="card">
          <h2>{t("status.output")}</h2>
          <Row
            label={t("status.mode")}
            value={s.output.mode !== "auto" ? valueLabel("output.resolution", s.output.mode) : dash}
          />
          <Row
            label={t("status.pixelClock")}
            value={s.output.pixelClockMHz ? `${formatNumber(s.output.pixelClockMHz, 3)} MHz` : dash}
          />
          <Row
            label={t("status.vfreq")}
            value={s.output.rateHz ? `${formatNumber(s.output.rateHz, 3)} Hz` : dash}
          />
          <Row label={t("status.frameLock")} value={lock} />
        </dl>
        <dl class="card">
          <h2>{t("status.board")}</h2>
          <Row label={t("status.version")} value={s.board.version} />
          <Row label={t("status.ip")} value={s.board.ip} />
          <Row label={t("status.rssi")} value={s.board.rssi ? `${s.board.rssi} dBm` : dash} />
          <Row
            label={t("status.clockGen")}
            value={s.board.clockGen ? t("status.present") : t("status.absent")}
          />
          <Row label={t("status.heap")} value={`${formatNumber(s.board.heapFree)} B`} />
          <Row label={t("status.uptime")} value={uptimeText(s.board.uptimeS)} />
        </dl>
      </div>
    </section>
  );
}
