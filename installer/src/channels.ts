// What this site can install, read from its latest.json (tools/release/installer_site.py): one
// ESP Web Tools manifest per channel (ADR 0012).

export type ChannelName = "stable" | "beta";

export interface Offer {
  channel: ChannelName;
  manifest: string;
  version: string;
  notes?: string;
}

const MANIFESTS: Readonly<Record<ChannelName, string>> = {
  stable: "manifest.json",
  beta: "manifest-beta.json",
};

/** The channels staged on this site, stable first; empty when nothing is. */
export function offers(latest: unknown): Offer[] {
  if (typeof latest !== "object" || latest === null) return [];
  const data = latest as Record<string, unknown>;
  if (data.format !== "gbsr-latest") return [];
  const found: Offer[] = [];
  for (const channel of ["stable", "beta"] as const) {
    const entry = data[channel] as { version?: unknown; notes?: unknown } | undefined;
    if (typeof entry?.version !== "string") continue;
    found.push({
      channel,
      manifest: MANIFESTS[channel],
      version: entry.version,
      ...(typeof entry.notes === "string" ? { notes: entry.notes } : {}),
    });
  }
  return found;
}

export async function fetchOffers(): Promise<Offer[]> {
  try {
    const response = await fetch("latest.json", { cache: "no-store" });
    return response.ok ? offers(await response.json()) : [];
  } catch {
    return [];
  }
}
