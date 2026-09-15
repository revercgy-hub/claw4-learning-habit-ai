#!/usr/bin/env python3
"""Host Reuse Fingerprint - decide whether prior Host Gate evidence may be reused.

Rule (A05 task book, reviewer ruling #2): reuse is judged by whether the ACTUAL
inputs are identical, not by whether the commit SHA is identical. The fingerprint
must cover:

  * Host sources and tests        firmware/**, integration/**, tools/dev/**
  * the gate scripts themselves   (inside tools/dev/**, hence covered)
  * the real compile/link args    pinned literals below
  * TOOLCHAIN IDENTITY            the key compile/link programs AND their
                                  runtime libraries -- hashing only the two g++
                                  launchers does NOT characterise the toolchain

Usage:

  # write the baseline the first time, or after the fingerprint tool itself changes
  python tools/dev/host-reuse-fingerprint.py --write-baseline out/host-fingerprint.json

  # later: is the current tree still covered by that baseline?
  python tools/dev/host-reuse-fingerprint.py --check out/host-fingerprint.json

Exit status: 0 same, 1 different / unreadable baseline, 2 usage error.
Whenever this tool itself changes, its own file hash changes, which changes the
file set -- so the baseline MUST be regenerated instead of reused.
"""

import argparse
import hashlib
import json
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

FILE_TREES = ("firmware", "integration", "tools/dev")
EXCLUDE_DIRS = {".git", "__pycache__", "out"}
EXCLUDE_EXT = {".o", ".obj", ".exe", ".a", ".so", ".dll"}

NATIVE_ROOT = r"E:\workbuddy\toolchains\w64devkit-2.9.1"
CROSS_ROOT = (r"E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf"
              r"\esp-14.2.0_20260121\riscv32-esp-elf")

# Driver programs (bin/).
NATIVE_PROGRAMS = ["g++.exe", "gcc.exe", "c++.exe", "cpp.exe", "ld.exe", "as.exe",
                   "ar.exe", "nm.exe", "objdump.exe", "size.exe"]
CROSS_PROGRAMS = ["riscv32-esp-elf-g++", "riscv32-esp-elf-gcc", "riscv32-esp-elf-cpp",
                  "riscv32-esp-elf-ld", "riscv32-esp-elf-as", "riscv32-esp-elf-ar",
                  "riscv32-esp-elf-nm", "riscv32-esp-elf-objdump", "riscv32-esp-elf-size"]

# The ACTUAL code generators / linkers, not just the driver front-ends.
NATIVE_INTERNALS = ["libexec/gcc/x86_64-w64-mingw32/16.2.0/cc1.exe",
                    "libexec/gcc/x86_64-w64-mingw32/16.2.0/cc1plus.exe",
                    "libexec/gcc/x86_64-w64-mingw32/16.2.0/collect2.exe"]
CROSS_INTERNALS = ["libexec/gcc/riscv32-esp-elf/14.2.0/cc1.exe",
                   "libexec/gcc/riscv32-esp-elf/14.2.0/cc1plus.exe",
                   "libexec/gcc/riscv32-esp-elf/14.2.0/collect2.exe"]

# Runtime / archive identity. NOTE: w64devkit ships NO DLLs (it is a statically
# linked single-file distribution -- `g++ -print-file-name=libstdc++-6.dll`
# returns the bare name, and there is no .dll anywhere in the tree), so the
# meaningful native runtime identity is its static archives instead.
NATIVE_RUNTIME = ["lib/gcc/x86_64-w64-mingw32/16.2.0/libstdc++.a",
                  "lib/gcc/x86_64-w64-mingw32/16.2.0/libgcc.a",
                  "lib/gcc/x86_64-w64-mingw32/16.2.0/libsupc++.a",
                  "lib/libwinpthread.a"]
CROSS_RUNTIME = ["lib/gcc/riscv32-esp-elf/14.2.0/libgcc.a",
                 "picolibc/riscv32-esp-elf/lib/libstdc++.a",
                 "picolibc/riscv32-esp-elf/lib/libm.a",
                 "picolibc/riscv32-esp-elf/lib/libc.a",
                 "picolibc/riscv32-esp-elf/lib/libsupc++.a"]

ARGS = {
    "compile": "-std=c++17 -Wall -Wextra -Werror -I firmware/main -I firmware/tests -I integration",
    "link": "-std=c++17 -Wall -Wextra -Werror -I firmware/main -I firmware/tests -I integration <objs...>",
    "gate_script": "tools/dev/verify-host-cpp-tests.ps1",
    "interface_script": "tools/dev/verify-interface-contracts.ps1",
}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def collect_file_set():
    rows = []
    for tree in FILE_TREES:
        base = os.path.join(REPO_ROOT, tree.replace("/", os.sep))
        if not os.path.isdir(base):
            continue
        for r, dirs, fs in os.walk(base):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for f in fs:
                if os.path.splitext(f)[1].lower() in EXCLUDE_EXT:
                    continue
                full = os.path.join(r, f)
                rel = os.path.relpath(full, REPO_ROOT).replace("\\", "/")
                rows.append((rel, sha256_file(full)))
    rows.sort()
    listing = "\n".join("%s  %s" % (h, p) for p, h in rows)
    return rows, hashlib.sha256(listing.encode()).hexdigest()


def collect_toolchain():
    entries = {}

    def add(kind, label, path):
        if os.path.isfile(path):
            entries[label] = {"kind": kind, "path": path.replace("\\", "/"),
                              "sha256": sha256_file(path), "bytes": os.path.getsize(path)}
        else:
            entries[label] = {"kind": kind, "path": path.replace("\\", "/"), "sha256": "MISSING"}

    for p in NATIVE_PROGRAMS:
        add("native-program", "native:" + p, os.path.join(NATIVE_ROOT, "bin", p))
    for rel in NATIVE_INTERNALS:
        add("native-internal", "native-int:" + os.path.basename(rel),
            os.path.join(NATIVE_ROOT, rel.replace("/", os.sep)))
    for rel in NATIVE_RUNTIME:
        add("native-runtime", "native-rt:" + os.path.basename(rel),
            os.path.join(NATIVE_ROOT, rel.replace("/", os.sep)))
    for p in CROSS_PROGRAMS:
        add("cross-program", "cross:" + p, os.path.join(CROSS_ROOT, "bin", p + ".exe"))
    for rel in CROSS_INTERNALS:
        add("cross-internal", "cross-int:" + os.path.basename(rel),
            os.path.join(CROSS_ROOT, rel.replace("/", os.sep)))
    for rel in CROSS_RUNTIME:
        add("cross-runtime", "cross-rt:" + os.path.basename(rel),
            os.path.join(CROSS_ROOT, rel.replace("/", os.sep)))

    present = {k: v["sha256"] for k, v in entries.items() if v["sha256"] != "MISSING"}
    digest = hashlib.sha256(json.dumps(present, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return entries, digest


def compute():
    rows, trees_hash = collect_file_set()
    toolchain, toolchain_hash = collect_toolchain()
    args_hash = hashlib.sha256(json.dumps(ARGS, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    canonical = json.dumps(
        {"args": args_hash, "trees": trees_hash, "toolchain": toolchain_hash},
        sort_keys=True, separators=(",", ":"),
    )
    return {
        "definition": "sha256(json.dumps({args, trees, toolchain}, sort_keys=True, separators=(',',':')))",
        "repo_root": REPO_ROOT.replace("\\", "/"),
        "file_set": list(FILE_TREES),
        "file_count": len(rows),
        "trees_hash": trees_hash,
        "args": ARGS,
        "args_hash": args_hash,
        "toolchain_hash": toolchain_hash,
        "toolchain": toolchain,
        "canonical_json": canonical,
        "HOST_REUSE_FINGERPRINT": hashlib.sha256(canonical.encode()).hexdigest(),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--write-baseline", metavar="PATH")
    ap.add_argument("--check", metavar="PATH")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    if not args.write_baseline and not args.check:
        ap.error("give --write-baseline PATH or --check PATH")

    cur = compute()
    print("files                : %d" % cur["file_count"])
    print("trees_hash           : %s" % cur["trees_hash"])
    print("args_hash            : %s" % cur["args_hash"])
    print("toolchain_hash       : %s" % cur["toolchain_hash"])
    missing = [k for k, v in cur["toolchain"].items() if v["sha256"] == "MISSING"]
    if missing:
        print("toolchain entries MISSING from disk: %d" % len(missing))
        if args.verbose:
            for m in missing:
                print("  - %s  (%s)" % (m, cur["toolchain"][m]["path"]))
    print("HOST_REUSE_FINGERPRINT: %s" % cur["HOST_REUSE_FINGERPRINT"])

    if args.write_baseline:
        with open(args.write_baseline, "w", encoding="utf-8", newline="\n") as f:
            json.dump(cur, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("baseline written: %s" % args.write_baseline)
        return 0

    if not os.path.isfile(args.check):
        print("FAIL: baseline not found: %s" % args.check)
        return 1
    old = json.load(open(args.check, encoding="utf-8"))

    changed = []
    for key in ("trees_hash", "args_hash", "toolchain_hash", "HOST_REUSE_FINGERPRINT"):
        if old.get(key) != cur.get(key):
            changed.append(key)

    if changed:
        print("")
        print("RESULT: DIFFERENT -> rerun the full Host Gate, prior evidence is NOT reusable")
        print("changed fields: %s" % ", ".join(changed))
        if "toolchain_hash" in changed:
            oldt = old.get("toolchain", {})
            for k in sorted(set(oldt) | set(cur["toolchain"])):
                a = oldt.get(k, {}).get("sha256")
                b = cur["toolchain"].get(k, {}).get("sha256")
                if a != b:
                    print("  toolchain changed: %s  %s -> %s" % (k, (a or "-")[:16], (b or "-")[:16]))
        return 1

    print("")
    print("RESULT: SAME -> prior Host Gate evidence may be reused")
    print("(still record the git diff between the two commits in the report)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
