#!/usr/bin/env python3
"""Verify the actual CONTENT of managed_components against dependencies.lock.

Why this exists: comparing the lock's `component_hash` with the on-disk
`.component_hash` file only proves the two RECORDS agree. It does NOT prove the
component files are unmodified. This tool recomputes the directory content hash
with ESP-IDF's own algorithm -- the same one the component manager uses in
STRICT_CHECKSUM mode -- and compares it with the lock.

    hash_dir(root) = sha256 over sorted files of: update(relpath); update(sha256(bytes))
    with the component manifest's include/exclude sets applied and
    .component_hash / .checksums.json excluded.

MUST be run with the IDF python environment (it provides idf_component_tools):

    E:/workbuddy/claw4-idf-tools/python_env/idf5.5_py3.12_env/Scripts/python.exe \
        tools/dev/verify-component-content.py --lock E:/c/dependencies.lock \
        --components E:/c/managed_components

Exit status is NON-ZERO for every failure class below, with no silent skipping:
  * lock file missing / unreadable / zero components parsed
  * components directory missing
  * a lock entry whose directory is absent (only `idf` may be absent)
  * a lock entry with no component_hash
  * any content mismatch
  * any per-component exception

`--negative-control` copies one component, flips a single bit, and asserts the
check reports a mismatch -- a check that cannot fail proves nothing.
"""

import argparse
import hashlib
import os
import re
import shutil
import sys
import tempfile

try:
    from idf_component_tools.hash_tools.calculate import hash_dir
    from idf_component_tools.hash_tools.constants import CHECKSUMS_FILENAME, HASH_FILENAME
    from idf_component_tools.manager import ManifestManager
except ImportError as exc:  # pragma: no cover - environment guard
    print("FATAL: idf_component_tools not importable (%s)." % exc, file=sys.stderr)
    print("Run this tool with the IDF python environment:", file=sys.stderr)
    print("  .../python_env/idf5.5_py3.12_env/Scripts/python.exe", file=sys.stderr)
    sys.exit(2)

# The only entry that legitimately has no managed_components directory.
SKIPPABLE = {"idf"}


def component_dir_name(name):
    return name.replace("/", "__")


def parse_lock(path):
    """Parse the small YAML subset that dependencies.lock actually uses."""
    entries = {}
    cur = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f.read().splitlines():
            m = re.match(r"^  ([A-Za-z0-9_./\-]+):\s*$", line)
            if m:
                cur = m.group(1)
                entries[cur] = {}
                continue
            if cur is None:
                continue
            m2 = re.match(r"^    (\w+):\s*(\S+)", line)
            if m2:
                entries[cur][m2.group(1)] = m2.group(2).strip("'\"")
    return entries


def recompute(root):
    """Content hash using the component manager's own filters."""
    manifest = ManifestManager(root, "verify").load()
    exclude = set(manifest.exclude_set)
    exclude.add(f"**/{HASH_FILENAME}")
    exclude.add(f"**/{CHECKSUMS_FILENAME}")
    return hash_dir(
        root,
        use_gitignore=manifest.use_gitignore,
        include=manifest.include_set,
        exclude=exclude,
        exclude_default=False,
    )


def verify(lock_path, comp_dir, verbose):
    if not os.path.isfile(lock_path):
        print("FAIL: lock file not found: %s" % lock_path)
        return 1
    if not os.path.isdir(comp_dir):
        print("FAIL: components directory not found: %s" % comp_dir)
        return 1

    entries = parse_lock(lock_path)
    if not entries:
        print("FAIL: parsed 0 components from %s (wrong parser or corrupt lock?)" % lock_path)
        return 1

    failures = []
    matched = 0

    for name in sorted(entries):
        expected = entries[name].get("component_hash", "")
        path = os.path.join(comp_dir, component_dir_name(name))

        if name in SKIPPABLE:
            if verbose:
                print("  skip     %-40s (pseudo-entry, no directory expected)" % name)
            continue
        if not os.path.isdir(path):
            failures.append("%s: directory missing (%s)" % (name, path))
            continue
        if not expected:
            failures.append("%s: no component_hash in lock" % name)
            continue
        try:
            actual = recompute(path)
        except Exception as exc:  # noqa: BLE001 - report, never skip
            failures.append("%s: exception %s: %s" % (name, type(exc).__name__, exc))
            continue
        if actual == expected:
            matched += 1
            if verbose:
                print("  ok       %-40s %s" % (name, actual[:16] + ".."))
        else:
            failures.append("%s: CONTENT MISMATCH lock=%s actual=%s" % (name, expected, actual))

    print("")
    print("lock                   : %s" % lock_path)
    print("components             : %s" % comp_dir)
    print("lock entries           : %d" % len(entries))
    print("content verified OK    : %d" % matched)
    print("failures               : %d" % len(failures))
    for f in failures:
        print("  FAIL %s" % f)

    return 1 if failures else 0


def negative_control(comp_dir, verbose):
    """Assert the check can actually fail: flip one bit and expect a mismatch."""
    pick = None
    for name in sorted(os.listdir(comp_dir)):
        d = os.path.join(comp_dir, name)
        if not os.path.isdir(d) or not os.path.isfile(os.path.join(d, HASH_FILENAME)):
            continue
        size = sum(os.path.getsize(os.path.join(r, f)) for r, _x, fs in os.walk(d) for f in fs)
        if size < 400_000:
            pick = (name, d)
            break
    if pick is None:
        print("FAIL: no small component available for the negative control")
        return 1

    name, src = pick
    expected = open(os.path.join(src, HASH_FILENAME), "r", encoding="utf-8").read().strip()
    tmp = tempfile.mkdtemp(prefix="a05_negctl_")
    dst = os.path.join(tmp, name)
    try:
        shutil.copytree(src, dst)
        before = recompute(dst)

        victim = None
        for r, _x, fs in os.walk(dst):
            for f in sorted(fs):
                if f in (HASH_FILENAME, CHECKSUMS_FILENAME):
                    continue
                p = os.path.join(r, f)
                if os.path.getsize(p) > 0:
                    victim = p
                    break
            if victim:
                break
        if victim is None:
            print("FAIL: no victim file for the negative control")
            return 1

        original = open(victim, "rb").read()
        mutated = bytearray(original)
        mutated[0] ^= 0x01
        with open(victim, "wb") as f:
            f.write(bytes(mutated))
        after_flip = recompute(dst)

        with open(victim, "wb") as f:
            f.write(original)
        after_restore = recompute(dst)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    ok_before = before == expected
    detected = after_flip != expected
    ok_restore = after_restore == expected

    if verbose:
        print("  component          : %s" % name)
        print("  baseline == lock   : %s" % ok_before)
        print("  after 1-bit flip   : %s (detected=%s)" % (after_flip[:16] + "..", detected))
        print("  after restore      : %s" % ok_restore)
    print("negative control: detects a single-bit change = %s" % detected)
    return 0 if (ok_before and detected and ok_restore) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lock", default=r"E:\c\dependencies.lock")
    ap.add_argument("--components", default=r"E:\c\managed_components")
    ap.add_argument("--negative-control", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    rc = verify(args.lock, args.components, args.verbose)
    if args.negative_control:
        print("")
        rc |= negative_control(args.components, args.verbose)
    return rc


if __name__ == "__main__":
    sys.exit(main())
