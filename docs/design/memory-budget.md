# Memory budget

For firmware developers. The ESP8266 has ~80 KB of data RAM shared by static data, the Wi-Fi
stack and the heap; the heap, not static RAM, is what ran out first in the reference project
(idle ~15 KB, HTTP OTA failing below that — [lessons P9, P11](../research/gbs-control-lessons-platform.md)).

## Budgets

Enforced by `tools/size/check_budget.py` in CI against `tools/size/budget.json`.

| Quantity | Budget | Measured by | Why this number |
|---|---|---|---|
| Static RAM (`.data + .rodata + .bss`) | ≤ 40 KB | ELF section sizes | Leaves ≥ 40 KB for the SDK and heap |
| Growth of static RAM per change | ≤ 512 B | CI diff against `main` | Forces each change to pay for itself |
| IRAM (`.text + .text1`) | ≤ 32 KB | ELF | The core's IRAM limit with the default MMU layout |
| Flash (`.irom0.text` + `.rodata` in flash) | ≤ 800 KB | ELF | Sketch area is 1019 KB; keeps room for growth |
| Web UI bundle (gzip) | ≤ 64 KiB | `web/dist/index.html.gz` | Embedded in flash; transfer time on the loop |
| Idle free heap, one browser polling | ≥ 24 KB | `/api/status` `heapFree` on the bench | Above the reference's failing OTA threshold with margin |
| Minimum free heap during OTA upload | ≥ 12 KB | bench gate M3 | Upload needs one 2 KB chunk plus the TCP window |

The heap figures are bench gates, not CI checks; the status model publishes `heapFree` and
`heapMin` (low-water mark since boot) so every bench run reports them.

## Rules

- Constants and strings in flash (`PROGMEM`, `F()`), never `const` alone.
- Fixed-size rings for logs and journals; no `String`, `std::vector` growth or `new` after setup in
  the video path.
- Transient buffers live on the stack inside one handler and stay under 1 KB.
- JSON is written with bounded `snprintf_P` into a reused buffer, or streamed in chunks.
- The OLED frame buffer (1 KB) is the largest static object; any other static object above 512 B is
  listed below with its reason.

## Large static objects

| Object | Bytes | Reason |
|---|---|---|
| U8g2 frame buffer | 1024 | full-buffer mode for partial tile updates |
| Status model | ~256 | shared by all front ends |
| Event ring (32 × 8 B) | 256 | diagnostics without a serial cable |
| Write journal (64 × 6 B) | 384 | "who wrote this register" |
