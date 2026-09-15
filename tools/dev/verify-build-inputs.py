#!/usr/bin/env python3
"""Build-input manifest: freeze the repo-EXTERNAL inputs and re-verify them.

Policy (A05 task book §3, reviewer ruling #1): in-repo source/scripts/patches
must be committed and clean; inputs that live OUTSIDE the repo (IDF, toolchain,
managed_components, the isolated source copy, the SDKCONFIG copy, resources)
are NOT required to be committed -- but they must be frozen by a manifest that
records origin, version and hash, and re-verified before every build.

Tree hash definition (deterministic):
    sha256( "\n".join("<sha256>  <relpath>" for every file, sorted by relpath) )
  * relpaths use "/" and are relative to the entry root
  * files are hashed raw (these trees are not git-controlled; bytes are truth)
  * excludes are recorded in the entry itself, so the digest is reproducible

Usage:
  python tools/dev/verify-build-inputs.py --manifest <path> --write
  python tools/dev/verify-build-inputs.py --manifest <path> --check
Exit status: 0 ok, 1 any mismatch/missing/error, 2 usage error.
"""

import argparse
import hashlib
import json
import os
import sys

# Directories never worth hashing: build caches and VCS metadata.
DEFAULT_EXCLUDE_DIRS = {".git", "__pycache__", ".pytest_cache"}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def file_marker(path):
    """Content digest, with explicit markers for things that cannot be read.

    Some bundled toolchain trees contain entries Windows refuses to open as a
    regular file (e.g. `msys64/etc/mtab`, and symlink-like reparse points).
    Crashing would be wrong; silently skipping would be worse. So emit a
    deterministic marker that is recorded and reported instead.
    """
    if os.path.islink(path):
        return "SYMLINK->" + os.readlink(path)
    try:
        return sha256_file(path)
    except OSError as exc:
        return "UNREADABLE:%s" % exc.errno


def tree_hash(root, extra_exclude=()):
    """Return (digest, file_count, total_bytes, unreadable_paths)."""
    exclude = set(DEFAULT_EXCLUDE_DIRS) | set(extra_exclude)
    rows = []
    total = 0
    unreadable = []
    for r, dirs, fs in os.walk(root):
        dirs[:] = [d for d in dirs if d not in exclude]
        for f in fs:
            full = os.path.join(r, f)
            rel = os.path.relpath(full, root).replace("\\", "/")
            mark = file_marker(full)
            if mark.startswith(("UNREADABLE:", "SYMLINK->")):
                unreadable.append(rel + "  [" + mark + "]")
            rows.append((rel, mark))
            try:
                total += os.path.getsize(full)
            except OSError:
                pass
    rows.sort()
    digest = hashlib.sha256(
        "\n".join("%s  %s" % (h, p) for p, h in rows).encode()
    ).hexdigest()
    return digest, len(rows), total, unreadable


# ---------------------------------------------------------------------------
# The A05 build inputs. `dest` is where the entry was installed inside the
# isolated build root; `exclude` must be recorded so the digest reproduces.
# ---------------------------------------------------------------------------
INPUTS = [
    {
        "id": "idf",
        "kind": "external-tree",
        "origin": r"E:\workbuddy\esp-idf-5.5.4-ascii",
        "version": "5.5.4 (tools/cmake/version.cmake; no git metadata -> no SHA claim)",
        "used_as": "IDF_PATH",
    },
    {
        "id": "idf_tools",
        "kind": "external-tree",
        "origin": r"E:\workbuddy\claw4-idf-tools",
        "version": "cmake 3.30.2 / ninja 1.12.1 / riscv32-esp-elf esp-14.2.0_20260121 / python 3.12.14",
        "used_as": "IDF_TOOLS_PATH",
    },
    {
        "id": "managed_components",
        "kind": "external-tree-installed",
        "origin": r"E:\c\managed_components",
        "version": "82 components, per E:\\c\\dependencies.lock (sha256 90af1add...)",
        "used_as": "<isolated src>/managed_components",
        "dest": r"E:\claw4-a05-19fd979\src\managed_components",
    },
    {
        "id": "sdkconfig",
        "kind": "external-file-installed",
        "origin": r"E:\workbuddy\claw4-idf-cold-c5-20260906-frozen-20260912\sdkconfig",
        "version": "frozen C5 configuration",
        "used_as": "SDKCONFIG (explicit)",
        "dest": r"E:\claw4-a05-19fd979\src\sdkconfig",
    },
    {
        "id": "isolated_src",
        "kind": "replayed-tree",
        "origin": r"E:\claw4-a05-19fd979\src",
        "version": "pin ca3aa3fa + A01 + 19fd979 learning sync + A05 patch + frozen sdkconfig",
        "used_as": "project source (-C)",
        "exclude_dirs": ["managed_components", "build"],
    },
]


def measure(entry):
    """Return the measured identity of an entry (origin + installed dest)."""
    out = {"id": entry["id"], "kind": entry["kind"], "origin": entry["origin"],
           "version": entry.get("version", ""), "used_as": entry.get("used_as", "")}
    if entry.get("exclude_dirs"):
        out["exclude_dirs"] = entry["exclude_dirs"]

    kind = entry["kind"]
    if kind.endswith("-file-installed") or kind == "external-file":
        p = entry.get("dest") or entry["origin"]
        if not os.path.isfile(p):
            out["error"] = "file not found: %s" % p
            return out
        out["origin_sha256"] = sha256_file(entry["origin"]) if os.path.isfile(entry["origin"]) else None
        out["dest"] = entry.get("dest", "")
        out["dest_sha256"] = sha256_file(p)
        out["bytes"] = os.path.getsize(p)
        return out

    root = entry.get("dest") or entry["origin"]
    if not os.path.isdir(root):
        out["error"] = "directory not found: %s" % root
        return out
    digest, count, total, unreadable = tree_hash(entry["origin"], entry.get("exclude_dirs", ()))
    out["origin_tree_sha256"] = digest
    out["origin_file_count"] = count
    out["origin_bytes"] = total
    if unreadable:
        out["unreadable_or_symlink"] = unreadable
        out["unreadable_count"] = len(unreadable)
    if entry.get("dest"):
        d2, c2, t2, u2 = tree_hash(entry["dest"], entry.get("exclude_dirs", ()))
        out["dest"] = entry["dest"]
        out["dest_tree_sha256"] = d2
        out["dest_file_count"] = c2
        out["dest_bytes"] = t2
        if u2:
            out["dest_unreadable_or_symlink"] = u2
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--only", help="restrict to one entry id")
    args = ap.parse_args()
    if args.write == args.check:
        ap.error("give exactly one of --write / --check")

    entries = [e for e in INPUTS if (args.only is None or e["id"] == args.only)]
    results = []
    for e in entries:
        print("hashing %s ..." % e["id"], flush=True)
        results.append(measure(e))

    if args.write:
        doc = {
            "purpose": "A05 build inputs: repo-external inputs frozen by hash; re-verify before each build",
            "tree_hash_definition": 'sha256("\\n".join("<sha256>  <relpath>" sorted by relpath))',
            "policy_ref": "WB-A05-BUILD-001 §3 (input manifest) and reviewer ruling #1",
            "entries": results,
        }
        with open(args.manifest, "w", encoding="utf-8", newline="\n") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
            f.write("\n")
        bad = [r for r in results if r.get("error")]
        for r in results:
            print("  %-20s %s" % (r["id"], r.get("error") or
                                  (r.get("dest_tree_sha256") or r.get("dest_sha256") or r.get("origin_tree_sha256"))[:24] + ".."))
        print("manifest written: %s" % args.manifest)
        return 1 if bad else 0

    if not os.path.isfile(args.manifest):
        print("FAIL: manifest not found: %s" % args.manifest)
        return 1
    old = {e["id"]: e for e in json.load(open(args.manifest, encoding="utf-8"))["entries"]}

    failures = []
    for cur in results:
        ref = old.get(cur["id"])
        if ref is None:
            failures.append("%s: not present in manifest" % cur["id"])
            continue
        if cur.get("error"):
            failures.append("%s: %s" % (cur["id"], cur["error"]))
            continue
        for key in ("origin_tree_sha256", "origin_file_count", "origin_bytes",
                    "origin_sha256", "dest_tree_sha256", "dest_sha256", "bytes"):
            if key in ref and ref[key] != cur.get(key):
                failures.append("%s: %s changed  manifest=%s  now=%s"
                                % (cur["id"], key, ref[key], cur.get(key)))
        if ref.get("dest") and not cur.get("dest"):
            failures.append("%s: install destination missing" % cur["id"])

    print("")
    print("manifest : %s" % args.manifest)
    print("entries  : %d checked" % len(results))
    print("failures : %d" % len(failures))
    for f in failures:
        print("  FAIL %s" % f)
    if failures:
        print("RESULT: FAIL -- do NOT build; fix or re-freeze the inputs first")
        return 1
    print("RESULT: PASS -- every external input matches the frozen manifest")
    return 0


if __name__ == "__main__":
    sys.exit(main())
