#!/usr/bin/env python3
"""Safely rebuild an NVS image and append device backend provisioning.

This tool is intended for a local, controlled provisioning session.  It never
prints values from the source NVS image or the device secret.  The source image
is parsed into logical entries, rebuilt with ESP-IDF's official generator, and
checked to ensure every existing namespace/key remains present before the
additional ``learning_cfg`` namespace is accepted.
"""

from __future__ import annotations

import argparse
import base64
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any


NVS_TOOL_DIR = Path(__file__).resolve().parents[2] / ".." / "esp-idf-5.5.4-ascii" / "components" / "nvs_flash" / "nvs_partition_tool"
NVS_TOOL_DIR = NVS_TOOL_DIR.resolve()
GENERATOR_MODULE = "esp_idf_nvs_partition_gen"


def load_parser():
    sys.path.insert(0, str(NVS_TOOL_DIR))
    import nvs_parser  # type: ignore

    return nvs_parser


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="read-only NVS dump")
    parser.add_argument("output", type=Path, help="rebuilt NVS image")
    parser.add_argument("size", type=int, help="NVS partition size in bytes")
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--child-id", required=True)
    parser.add_argument(
        "--secret-stdin",
        action="store_true",
        help="read the device secret from stdin; the value is never logged",
    )
    return parser.parse_args()


def collect_entries(source: Path) -> tuple[list[tuple[str, str, str, Any]], set[tuple[str, str]], set[str]]:
    nvs_parser = load_parser()
    partition = nvs_parser.NVS_Partition(source.name, bytearray(source.read_bytes()))
    namespaces: dict[int, str] = {}
    for page in partition.pages:
        for entry in page.entries:
            if entry.state != "Written" or entry.key is None:
                continue
            if entry.metadata["namespace"] == 0:
                if entry.metadata["type"] != "uint8_t":
                    raise ValueError("unexpected root namespace entry type")
                namespaces[int(entry.data["value"])] = entry.key

    entries_by_namespace: dict[str, list[tuple[str, str, str, Any]]] = {
        name: [] for name in namespaces.values()
    }

    rows: list[tuple[str, str, str, Any]] = []
    logical_keys: set[tuple[str, str]] = {
        (name, "<namespace>") for name in namespaces.values()
    }
    namespace_names = set(namespaces.values())

    for page in partition.pages:
        for entry in page.entries:
            if entry.state != "Written" or entry.key is None:
                continue
            namespace_id = entry.metadata["namespace"]
            if namespace_id == 0:
                continue
            if namespace_id not in namespaces:
                raise ValueError("entry refers to unknown namespace")
            namespace = namespaces[namespace_id]
            entry_type = entry.metadata["type"]

            if entry_type == "blob_index":
                # The paired blob_data entry contains the payload.  The
                # generator recreates the index from the payload.
                continue
            if entry_type in ("string", "blob_data"):
                payload = bytearray()
                for child in entry.children:
                    payload.extend(child.raw)
                payload = payload[: int(entry.data["size"])]
                if entry_type == "string":
                    payload = payload.split(b"\0", 1)[0]
                    value = payload.decode("utf-8")
                    row = (entry.key, "data", "string", value)
                else:
                    value = base64.b64encode(payload).decode("ascii")
                    row = (entry.key, "data", "base64", value)
            else:
                type_map = {
                    "uint8_t": "u8",
                    "int8_t": "i8",
                    "uint16_t": "u16",
                    "int16_t": "i16",
                    "uint32_t": "u32",
                    "int32_t": "i32",
                    "uint64_t": "u64",
                    "int64_t": "i64",
                }
                if entry_type not in type_map:
                    raise ValueError(f"unsupported existing NVS type: {entry_type}")
                row = (entry.key, "data", type_map[entry_type], entry.data["value"])

            entries_by_namespace[namespace].append(row)
            logical_keys.add((namespace, entry.key))

    for namespace in namespaces.values():
        rows.append((namespace, "namespace", "", ""))
        rows.extend(entries_by_namespace[namespace])

    return rows, logical_keys, namespace_names


def read_secret() -> str:
    secret = sys.stdin.readline().strip()
    if not secret:
        raise ValueError("device secret stdin was empty")
    return secret


def write_csv(path: Path, rows: list[tuple[str, str, str, Any]], args: argparse.Namespace, secret: str) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(("key", "type", "encoding", "value"))
        for row in rows:
            writer.writerow(row)
        writer.writerow(("learning_cfg", "namespace", "", ""))
        writer.writerow(("base_url", "data", "string", args.base_url))
        writer.writerow(("device_id", "data", "string", args.device_id))
        writer.writerow(("child_id", "data", "string", args.child_id))
        writer.writerow(("device_secret", "data", "string", secret))


def logical_key_set(path: Path) -> tuple[set[tuple[str, str]], set[str]]:
    nvs_parser = load_parser()
    partition = nvs_parser.NVS_Partition(path.name, bytearray(path.read_bytes()))
    namespaces: dict[int, str] = {}
    keys: set[tuple[str, str]] = set()
    for page in partition.pages:
        for entry in page.entries:
            if entry.state == "Written" and entry.key is not None and entry.metadata["namespace"] == 0:
                namespaces[int(entry.data["value"])] = entry.key
    for name in namespaces.values():
        keys.add((name, "<namespace>"))
    for page in partition.pages:
        for entry in page.entries:
            if entry.state == "Written" and entry.key is not None and entry.metadata["namespace"] != 0:
                name = namespaces.get(entry.metadata["namespace"])
                if name is not None and entry.metadata["type"] != "blob_index":
                    keys.add((name, entry.key))
    return keys, set(namespaces.values())


def main() -> int:
    args = parse_args()
    if args.size <= 0 or args.size % 4096:
        raise ValueError("size must be a positive multiple of 4096")
    if not args.secret_stdin:
        raise ValueError("--secret-stdin is required for local provisioning")
    secret = read_secret()
    rows, old_keys, old_namespaces = collect_entries(args.source)
    with tempfile.TemporaryDirectory(prefix="claw4-nvs-") as temp_dir:
        csv_path = Path(temp_dir) / "merged.csv"
        write_csv(csv_path, rows, args, secret)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        command = [
            sys.executable,
            "-m",
            GENERATOR_MODULE,
            "generate",
            "--version",
            "2",
            str(csv_path),
            str(args.output),
            str(args.size),
        ]
        result = subprocess.run(command, check=False, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"NVS generator failed with exit code {result.returncode}")

    if args.output.stat().st_size != args.size:
        raise RuntimeError("generated NVS image has an unexpected size")
    new_keys, new_namespaces = logical_key_set(args.output)
    if not old_namespaces.issubset(new_namespaces):
        raise RuntimeError("generated image dropped an existing namespace")
    if not old_keys.issubset(new_keys):
        raise RuntimeError("generated image dropped an existing namespace/key")
    expected = {
        ("learning_cfg", "<namespace>"),
        ("learning_cfg", "base_url"),
        ("learning_cfg", "device_id"),
        ("learning_cfg", "child_id"),
        ("learning_cfg", "device_secret"),
    }
    if not expected.issubset(new_keys):
        raise RuntimeError("generated image is missing provisioning keys")
    print(
        f"validated: preserved_namespaces={len(old_namespaces)} "
        f"preserved_entries={len(old_keys)} added_entries=5 size={args.size}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
