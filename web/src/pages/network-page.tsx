import { useEffect, useState } from "preact/hooks";
import { fetchWifi, joinWifi, scanWifi } from "../api/endpoints";
import type { ScanResult, WifiStatus } from "../api/types";
import type { MessageKey } from "../i18n";
import { t } from "../i18n";

const AP_PASSWORD = "gbsreimagined"; // documented in the user guide (user decision, 2026-10-13)
const SCAN_POLL_MS = 1000;
const SCAN_GIVE_UP = 20;

function describe(w: WifiStatus): string {
  if (w.mode === "joined") return t("net.joined", { ssid: w.ssid, ip: w.ip });
  if (w.mode === "joining") return t("net.joining", { ssid: w.ssid });
  return t("net.setup", { ap: w.ap.ssid, password: AP_PASSWORD });
}

export function NetworkPage() {
  const [wifi, setWifi] = useState<WifiStatus | null>(null);
  const [networks, setNetworks] = useState<NonNullable<ScanResult["networks"]>>([]);
  const [scanning, setScanning] = useState(false);
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [note, setNote] = useState<MessageKey | null>(null);

  useEffect(() => {
    let live = true;
    const refresh = async () => {
      try {
        const w = await fetchWifi();
        if (live) setWifi(w);
      } catch {
        // unreachable: the connection pill says so
      }
    };
    void refresh();
    const timer = setInterval(refresh, 3000);
    return () => {
      live = false;
      clearInterval(timer);
    };
  }, []);

  const scan = async () => {
    setScanning(true);
    for (let i = 0; i < SCAN_GIVE_UP; i++) {
      try {
        const r = await scanWifi();
        if (!r.scanning) {
          setNetworks(r.networks ?? []);
          break;
        }
      } catch {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, SCAN_POLL_MS));
    }
    setScanning(false);
  };

  const join = async (e: Event) => {
    e.preventDefault();
    try {
      const r = await joinWifi(ssid, password);
      setNote(r.ok ? "net.joinNote" : "action.failed");
    } catch {
      setNote("action.failed");
    }
  };

  return (
    <section>
      {wifi && <p class="headline">{describe(wifi)}</p>}
      <div class="card">
        <button type="button" disabled={scanning} onClick={scan}>
          {scanning ? t("net.scanning") : t("net.scan")}
        </button>
        <ul class="networks">
          {networks.map((n) => (
            <li key={n.ssid}>
              <button type="button" class="link" onClick={() => setSsid(n.ssid)}>
                {n.ssid}
              </button>
              <span class="muted">
                {n.rssi} dBm{n.secure ? " 🔒" : ""}
              </span>
            </li>
          ))}
        </ul>
        <form class="join" onSubmit={join}>
          <label>
            {t("net.ssid")}
            <input
              value={ssid}
              maxLength={32}
              autoComplete="off"
              onInput={(e) => setSsid(e.currentTarget.value)}
            />
          </label>
          <label>
            {t("net.password")}
            <input
              type="password"
              value={password}
              maxLength={64}
              onInput={(e) => setPassword(e.currentTarget.value)}
            />
          </label>
          <button type="submit" class="primary" disabled={ssid.length === 0}>
            {t("net.join")}
          </button>
        </form>
        {note && <p class="muted">{t(note)}</p>}
      </div>
    </section>
  );
}
