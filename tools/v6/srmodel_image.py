"""Read-only parser and verifier for the pinned pack_model.py image format."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


MAX_INPUT_BYTES = 64 * 1024 * 1024
MAX_MODELS = 256
MAX_FILES = 4096
MAX_FILES_PER_MODEL = MAX_FILES
U32 = struct.Struct("<I")
MODEL_PREFIX_SIZE = 36
FILE_ENTRY_SIZE = 40


class ImageError(ValueError):
    """A sanitized format or bound failure identified by a stable code."""

    def __init__(self, code):
        super().__init__(code)
        self.code = code


def _u32(data, offset):
    if offset < 0 or offset + 4 > len(data):
        raise ImageError("truncated_header")
    return U32.unpack_from(data, offset)[0]


def _name(raw):
    """Decode one fixed-width, NUL-padded ASCII name without exposing bytes."""
    value = raw.split(b"\0", 1)[0]
    if not value or (b"\0" in raw and any(raw[len(value) + 1:])):
        raise ImageError("invalid_name")
    try:
        return value.decode("ascii")
    except UnicodeDecodeError as exc:
        raise ImageError("non_ascii_name") from exc


def parse_image(data, max_input_bytes=MAX_INPUT_BYTES):
    """Parse a complete model image; trailing partition fill is reported as tail."""
    if len(data) > max_input_bytes:
        raise ImageError("input_too_large")
    if len(data) < 4:
        raise ImageError("truncated_header")

    model_count = _u32(data, 0)
    if model_count > MAX_MODELS:
        raise ImageError("model_count_limit")
    offset = 4
    models = []
    seen_models = set()
    all_files = 0
    ranges = []

    for _ in range(model_count):
        if offset + MODEL_PREFIX_SIZE > len(data):
            raise ImageError("truncated_header")
        name = _name(data[offset:offset + 32])
        offset += 32
        if name in seen_models:
            raise ImageError("duplicate_model_name")
        seen_models.add(name)
        file_count = _u32(data, offset)
        offset += 4
        if file_count > MAX_FILES_PER_MODEL:
            raise ImageError("file_count_limit")
        all_files += file_count
        if all_files > MAX_FILES:
            raise ImageError("file_count_limit")

        files = []
        seen_files = set()
        for _ in range(file_count):
            if offset + FILE_ENTRY_SIZE > len(data):
                raise ImageError("truncated_header")
            file_name = _name(data[offset:offset + 32])
            start = _u32(data, offset + 32)
            size = _u32(data, offset + 36)
            offset += FILE_ENTRY_SIZE
            if file_name in seen_files:
                raise ImageError("duplicate_file_name")
            seen_files.add(file_name)
            files.append({"name": file_name, "offset": start, "size": size})
            ranges.append((start, start + size))
        models.append({"name": name, "files": files})

    header_end = offset
    canonical_end = header_end
    for start, end in ranges:
        if start < header_end:
            raise ImageError("payload_before_header_end")
        if end > len(data):
            raise ImageError("payload_out_of_bounds")
        canonical_end = max(canonical_end, end)

    nonempty_ranges = sorted((start, end) for start, end in ranges if end > start)
    for previous, current in zip(nonempty_ranges, nonempty_ranges[1:]):
        if current[0] < previous[1]:
            raise ImageError("overlapping_payloads")

    # A valid packed image stores payloads contiguously after the whole header.
    # Gaps are rejected so the canonical image boundary is unambiguous.
    cursor = header_end
    for start, end in nonempty_ranges:
        if start != cursor:
            raise ImageError("payload_gap")
        cursor = end

    for model in models:
        for item in model["files"]:
            start, size = item.pop("offset"), item["size"]
            item["sha256"] = hashlib.sha256(data[start:start + size]).hexdigest()

    return {
        "catalog": models,
        "canonical_size": canonical_end,
        "tailbytes": len(data) - canonical_end,
    }


def compare_images(expected, actual, required_model, max_input_bytes=MAX_INPUT_BYTES):
    """Return allowlisted summaries and a comparison status; never emit payload data."""
    expected_info = parse_image(expected, max_input_bytes=max_input_bytes)
    actual_info = parse_image(actual, max_input_bytes=max_input_bytes)
    if expected_info["tailbytes"] != 0:
        raise ImageError("expected_image_has_tail")
    expected_names = {model["name"] for model in expected_info["catalog"]}
    actual_names = {model["name"] for model in actual_info["catalog"]}
    if required_model not in expected_names:
        raise ImageError("required_model_missing_expected")

    if required_model not in actual_names:
        status = "MISMATCH"
        reason = "required_model_missing_actual"
    elif expected_info["catalog"] != actual_info["catalog"]:
        status = "MISMATCH"
        reason = "catalog_mismatch"
    elif len(actual) < len(expected) or actual[:len(expected)] != expected:
        status = "MISMATCH"
        reason = "image_prefix_mismatch"
    else:
        status = "MATCH"
        reason = None

    return {
        "status": status,
        "reason": reason,
        "expected": expected_info["catalog"],
        "actual": actual_info["catalog"],
        "tailbytes": actual_info["tailbytes"],
    }


def _read_bounded(path):
    try:
        with Path(path).open("rb") as stream:
            data = stream.read(MAX_INPUT_BYTES + 1)
    except OSError as exc:
        raise ImageError("input_unavailable") from exc
    if len(data) > MAX_INPUT_BYTES:
        raise ImageError("input_too_large")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("expected", type=Path, help="built srmodels.bin")
    parser.add_argument("actual", type=Path, help="private full partition dump")
    parser.add_argument("--required-model", required=True)
    args = parser.parse_args(argv)
    try:
        result = compare_images(_read_bounded(args.expected), _read_bounded(args.actual),
                               args.required_model)
    except ImageError as exc:
        result = {"status": "INVALID", "reason": exc.code}
        print(json.dumps(result, separators=(",", ":")))
        return 2
    print(json.dumps(result, separators=(",", ":")))
    return 0 if result["status"] == "MATCH" else 1


if __name__ == "__main__":
    sys.exit(main())
