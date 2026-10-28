"""Cache of oracle results shared by tests/diff and tests/corpus.

The oracle is the pinned compiler's gcc build of a `.orb` program. Its
exit status, stdout and stderr depend only on the program bytes and the
compiler bytes, so a key over both makes a hit equivalent to a rebuild.
A hit skips the oracle build and the oracle run. Only completed runs are
stored: a build failure or a timeout is never cached.
"""

from __future__ import annotations

import base64
import hashlib
import json
import os
from pathlib import Path

SCHEMA = "r6-oracle-cache/1"


def sha256_bytes(*parts: bytes) -> str:
    digest = hashlib.sha256()
    for part in parts:
        digest.update(hashlib.sha256(part).digest())
    return digest.hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


class OracleCache:
    def __init__(self, path: Path, enabled: bool = True):
        self.path = path
        self.enabled = enabled
        self.entries = self._read() if enabled else {}
        self.fresh = {}
        self.hits = 0
        self.misses = 0

    def _read(self):
        try:
            with open(self.path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (OSError, ValueError):
            return {}
        if data.get("schema") != SCHEMA:
            return {}
        return data.get("entries", {})

    def get(self, key: str):
        """(exit, stdout bytes, stderr bytes) or None."""
        if not self.enabled:
            return None
        entry = self.entries.get(key)
        if entry is None:
            self.misses += 1
            return None
        self.hits += 1
        return (entry["exit"], base64.b64decode(entry["stdout"]),
                base64.b64decode(entry["stderr"]))

    def put(self, key: str, exit_code: int, stdout: bytes, stderr: bytes):
        if not self.enabled:
            return
        entry = {"exit": exit_code,
                 "stdout": base64.b64encode(stdout).decode("ascii"),
                 "stderr": base64.b64encode(stderr).decode("ascii")}
        self.entries[key] = entry
        self.fresh[key] = entry

    def save(self):
        """Merge this run's entries into the file another runner may share."""
        if not self.enabled or not self.fresh:
            return
        entries = self._read()
        entries.update(self.fresh)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temp = self.path.with_name(f"{self.path.name}.{os.getpid()}.tmp")
        with open(temp, "w", encoding="utf-8", newline="\n") as handle:
            json.dump({"schema": SCHEMA, "entries": entries}, handle,
                      sort_keys=True)
            handle.write("\n")
        os.replace(temp, self.path)

    def summary(self) -> str:
        if not self.enabled:
            return "oracle cache: off"
        return f"oracle cache: {self.hits} hits, {self.misses} misses ({self.path})"
