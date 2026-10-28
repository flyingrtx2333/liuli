#!/usr/bin/env python3
"""Stage fixed official ATK runtime locally. Never replaces system libraries.
The 2.56.2 SpiCache cleanup bug is fixed upstream by c4ddb104fb25ba9b1f839cea28f280bb3a4146ba.
Use only a process-local LD_LIBRARY_PATH pointing to the printed directory.
"""
import hashlib
from pathlib import Path
import subprocess
import urllib.request

BASE = Path(__file__).resolve().parent.parent / "build"
PACKAGES = {
    "libatk-bridge2.0-0t64_2.62.1-1_amd64.deb": "3b1f3c2f55e5f46e6eb38d3303e135f6e5976190da332bb4a7679fd1b57415f8",
    "libatk1.0-0t64_2.62.1-1_amd64.deb": "39171e7a10f682c89a75063166cef295b82d606fed587a9dccf49f4373efed6f",
    "libatspi2.0-0t64_2.62.1-1_amd64.deb": "cb528d2a550b39d70c80f0410d5c5d0a9b17b0a32ec55708e1c6af8a3a9d275a",
}
(BASE / "a11y-sdk").mkdir(parents=True, exist_ok=True)
for name, digest in PACKAGES.items():
    archive = BASE / "a11y-sdk" / name
    if not archive.exists():
        with urllib.request.urlopen("https://deb.debian.org/debian/pool/main/a/at-spi2-core/" + name, timeout=60) as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"SHA256 mismatch: {archive}")
    subprocess.run(["dpkg-deb", "-x", str(archive), str(BASE / "a11y-runtime")], check=True)
print(BASE / "a11y-runtime/usr/lib/x86_64-linux-gnu")
