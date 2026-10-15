import { HttpResponse, http } from "msw";
import { setupServer } from "msw/node";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { handlers } from "../src/mocks/handlers";
import { sha256Hex } from "../src/update/sha256";
import {
  ChecksumError,
  downloadOffer,
  fetchLatest,
  pickOffer,
  uploadFirmware,
} from "../src/update/update";
import { compareVersions } from "../src/update/version";

const text = (s: string) => new TextEncoder().encode(s);

// A gzip header with our comment, padded to 64 bytes: what the mock board checks.
const PAYLOAD = (() => {
  const head = [0x1f, 0x8b, 8, 0x10, 0, 0, 0, 0, 2, 255];
  const bytes = new Uint8Array(64);
  bytes.set(head);
  bytes.set(text("gbsr board=gbsc version=1.0.0"), head.length);
  return bytes;
})();

describe("sha256", () => {
  it("matches the FIPS 180-4 examples", () => {
    expect(sha256Hex(text(""))).toBe(
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    );
    expect(sha256Hex(text("abc"))).toBe(
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    );
    expect(sha256Hex(text("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))).toBe(
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
    );
  });

  it("handles inputs around the block boundary and a large one", () => {
    // 55, 56 and 64 bytes straddle the padding rule; values from a reference implementation.
    expect(sha256Hex(new Uint8Array(55))).toBe(
      "02779466cdec163811d078815c633f21901413081449002f24aa3e80f0b88ef7",
    );
    expect(sha256Hex(new Uint8Array(56))).toBe(
      "d4817aa5497628e7c77e6b606107042bbba3130888c5f47a375e6179be789fbb",
    );
    expect(sha256Hex(new Uint8Array(64))).toBe(
      "f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b",
    );
    expect(sha256Hex(new Uint8Array(1_000_000).fill(0x61))).toBe(
      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
    );
  });
});

describe("versions", () => {
  it("orders releases and pre-releases", () => {
    expect(compareVersions("1.0.0", "0.9.9")).toBeGreaterThan(0);
    expect(compareVersions("v1.2.0", "1.10.0")).toBeLessThan(0);
    expect(compareVersions("1.0.0-beta.2", "1.0.0")).toBeLessThan(0);
    expect(compareVersions("1.0.0-beta.10", "1.0.0-beta.2")).toBeGreaterThan(0);
    expect(compareVersions("1.0.0", "1.0.0")).toBe(0);
    expect(compareVersions("0.1.0-dev", "0.1.0")).toBeLessThan(0);
    expect(compareVersions("not a version", "1.0.0")).toBe(0);
  });
});

describe("updating the board", () => {
  const server = setupServer(
    ...handlers,
    http.get("http://board.test/site/latest.json", () =>
      HttpResponse.json({
        format: "gbsr-latest",
        version: 1,
        stable: {
          version: "1.0.0",
          notes: "https://example.test/notes",
          files: { gbsc: { ota: "firmware/a.ota.bin.gz", sha256: sha256Hex(PAYLOAD), size: 64 } },
        },
        beta: {
          version: "1.1.0-beta.1",
          files: { gbsc: { ota: "firmware/b.ota.bin.gz", sha256: "00", size: 64 } },
        },
      }),
    ),
    http.get("http://board.test/site/firmware/a.ota.bin.gz", () =>
      HttpResponse.arrayBuffer(PAYLOAD.slice().buffer),
    ),
    http.get("http://board.test/site/firmware/b.ota.bin.gz", () =>
      HttpResponse.arrayBuffer(PAYLOAD.slice().buffer),
    ),
  );
  beforeAll(() => server.listen({ onUnhandledFrame: "error" }));
  afterAll(() => server.close());

  it("finds the newest version for this board, betas only when asked", async () => {
    const latest = await fetchLatest("http://board.test/site/");
    expect(pickOffer(latest, "0.1.0", false)?.version).toBe("1.0.0");
    expect(pickOffer(latest, "0.1.0", true)?.version).toBe("1.1.0-beta.1");
    expect(pickOffer(latest, "1.0.0", false)).toBeNull();
  });

  it("downloads only what matches its checksum", async () => {
    const latest = await fetchLatest("http://board.test/site/");
    const good = pickOffer(latest, "0.1.0", false);
    if (!good) throw new Error("no offer");
    expect((await downloadOffer(good, "http://board.test/site/")).size).toBe(64);
    const bad = pickOffer(latest, "0.1.0", true);
    if (!bad) throw new Error("no offer");
    await expect(downloadOffer(bad, "http://board.test/site/")).rejects.toBeInstanceOf(
      ChecksumError,
    );
  });

  it("sends a file and reports the board's verdict", async () => {
    const ok = await uploadFirmware(new Blob([PAYLOAD]), "a.ota.bin.gz");
    expect(ok).toEqual({ ok: true, version: "1.0.0" });
    const raw = await uploadFirmware(new Blob([new Uint8Array([0xe9, 3, 2])]), "a.bin");
    expect(raw).toEqual({ ok: false, reason: "notGzip" });
  });
});
