# Browser installer (milestone M3)

A static page using ESP Web Tools 10.4.0 that flashes GBS Reimagined from Chrome or Edge over
Web Serial and provisions Wi-Fi through Improv (ADR 0012). CI will build it from the release
binaries: `manifest.json` (stable), `manifest-beta.json`, `latest.json` for update checks, and the
firmware images, all served from the same origin.

Until it is published, it will be possible to serve it locally (`localhost` is a secure context,
so Web Serial works there). Nothing lives here yet.
