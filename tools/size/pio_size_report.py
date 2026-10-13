"""PlatformIO post-build script: write the firmware's memory use to `size.json` beside the ELF.

PlatformIO's own size limits do not fail an ESP8266 build, so CI runs
`tools/size/check_budget.py` on this report instead (docs/design/memory-budget.md).
"""

import json
import subprocess
from pathlib import Path

Import("env")  # noqa: F821 - provided by PlatformIO's SCons environment

DRAM = (".data", ".rodata", ".bss")
IRAM = (".text", ".text1")
FLASH = (".irom0.text", ".irom.text")


def sections(size_tool: str, elf: str) -> dict[str, int]:
    out = subprocess.run([size_tool, "-A", elf], capture_output=True, text=True, check=True).stdout
    result: dict[str, int] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith(".") and parts[1].isdigit():
            result[parts[0]] = int(parts[1])
    return result


def report(source, target, env):  # noqa: ARG001 - SCons action signature
    elf = str(target[0])
    size_tool = env.subst("$SIZETOOL")
    sec = sections(size_tool, elf)
    data = {
        "static_ram": sum(sec.get(s, 0) for s in DRAM),
        "iram": sum(sec.get(s, 0) for s in IRAM),
        "flash": sum(sec.get(s, 0) for s in FLASH) + sum(sec.get(s, 0) for s in IRAM),
        "sections": sec,
    }
    out = Path(elf).with_name("size.json")
    out.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(
        f"size: static RAM {data['static_ram']} B, IRAM {data['iram']} B, "
        f"flash {data['flash']} B -> {out}"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", report)  # noqa: F821
