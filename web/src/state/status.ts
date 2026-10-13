import { signal } from "@preact/signals";
import { getJson } from "../api/client";
import type { Status } from "../api/types";
import { SCHEMA_HASH } from "../gen/settings.gen";

export const POLL_INTERVAL_MS = 1500;

export type Connection = "connecting" | "live" | "reconnecting" | "mismatch";

export const status = signal<Status | null>(null);
export const connection = signal<Connection>("connecting");

let timer: ReturnType<typeof setTimeout> | undefined;
let lastSeq = -1;

/** One poll; the next is scheduled only after this one settles, so polls never overlap. */
export async function pollOnce(): Promise<void> {
  try {
    const next = await getJson<Status>("/api/status");
    if (next.board.schema !== SCHEMA_HASH) {
      connection.value = "mismatch";
    } else {
      connection.value = "live";
    }
    if (next.seq !== lastSeq) {
      lastSeq = next.seq;
      status.value = next;
    }
  } catch {
    connection.value = status.value ? "reconnecting" : "connecting";
  }
}

export function startPolling(): void {
  const loop = async () => {
    await pollOnce();
    timer = setTimeout(loop, POLL_INTERVAL_MS);
  };
  void loop();
}

export function stopPolling(): void {
  if (timer !== undefined) clearTimeout(timer);
  timer = undefined;
}
