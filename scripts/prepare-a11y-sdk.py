#!/usr/bin/env python3
"""Extract pinned official Debian development headers locally; no apt/global installs.
Runtime libraries must already be available. This helper is for the amd64 Linux
cloud spike only; normal developer systems should use pkg-config instead.
"""
import hashlib
from pathlib import Path
import subprocess
import urllib.request

DEST = Path(__file__).resolve().parent.parent / "build/a11y-sdk"
PACKAGES = [
    ("a/at-spi2-core/libatk1.0-dev_2.46.0-5_amd64.deb", "73dd712a794ae52b93cc1f5fa7d55669a7c64fe4104c28a9c17995f1af44c14d"),
    ("a/at-spi2-core/libatk-bridge2.0-dev_2.46.0-5_amd64.deb", "fd1443ec9dbb974e1467aeae5395d0f1eea3cc0c147cbc65d5978483f4af2cb5"),
    ("a/at-spi2-core/libatspi2.0-dev_2.46.0-5_amd64.deb", "89f5c744c85ae6526bcfe64c81142132583a2afea75a3c849b7942ed0d57f2c6"),
    ("g/glib2.0/libglib2.0-dev_2.74.6-2+deb12u9_amd64.deb", "217766f170553e14884f7d67c0636b833da43c1e4a65dfe41268ba1292001ccd"),
    ("d/dbus/libdbus-1-dev_1.14.10-1~deb12u1_amd64.deb", "967a42e91eaa7d1885721b374a28cb0d30a0e63daaccf5e3594180bec6072c76"),
]
DEST.mkdir(parents=True, exist_ok=True)
for source, digest in PACKAGES:
    target = DEST / source.rsplit("/", 1)[1]
    if not target.exists():
        with urllib.request.urlopen("https://deb.debian.org/debian/pool/main/" + source, timeout=60) as response:
            target.write_bytes(response.read())
    if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"SHA256 mismatch: {target}")
    subprocess.run(["dpkg-deb", "-x", str(target), str(DEST / "root")], check=True)
print(DEST / "root")
