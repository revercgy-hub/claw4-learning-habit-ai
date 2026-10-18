"""Tests for the bounded, read-only packed model image verifier."""

import struct
import unittest

from srmodel_image import ImageError, compare_images, parse_image


def pack(models):
    """Build fixture records as [(model_name, [(file_name, payload), ...]), ...]."""
    header_size = 4 + sum(36 + len(files) * 40 for _, files in models)
    cursor = header_size
    header = [struct.pack("<I", len(models))]
    payloads = []
    for model_name, files in models:
        header.extend((model_name.encode("ascii").ljust(32, b"\0"),
                       struct.pack("<I", len(files))))
        for file_name, payload in files:
            header.extend((file_name.encode("ascii").ljust(32, b"\0"),
                           struct.pack("<II", cursor, len(payload))))
            payloads.append(payload)
            cursor += len(payload)
    return b"".join(header + payloads)


class SrmodelImageTests(unittest.TestCase):
    def test_valid_catalog_and_payload_digests(self):
        image = pack([("wake_a", [("model.bin", b"weights"), ("config", b"abc")]),
                      ("wake_b", [("model.bin", b"other")])])
        parsed = parse_image(image)
        self.assertEqual([model["name"] for model in parsed["catalog"]],
                         ["wake_a", "wake_b"])
        self.assertEqual(parsed["catalog"][0]["files"][0]["size"], 7)
        self.assertEqual(parsed["catalog"][0]["files"][0]["sha256"],
                         "9a129038d9a00aed0cf6a7ea059ca50a813449061ab87848cf1a13eafdf33b2c")
        self.assertEqual(parsed["tailbytes"], 0)

    def test_exact_expected_prefix_allows_partition_tail(self):
        expected = pack([("wn9_nihaoxiaozhi_tts", [("model", b"123456")])])
        actual = expected + b"\xff" * 128
        result = compare_images(expected, actual, "wn9_nihaoxiaozhi_tts")
        self.assertEqual(result["status"], "MATCH")
        self.assertEqual(result["tailbytes"], 128)
        self.assertEqual(set(result), {"status", "reason", "expected", "actual", "tailbytes"})

    def test_same_size_wrong_model_name_is_mismatch(self):
        expected = pack([("wn9_nihaoxiaozhi_tts", [("model", b"123456")])])
        actual = pack([("livewn9l_hai1tai4ling2", [("model", b"123456")])])
        result = compare_images(expected, actual, "wn9_nihaoxiaozhi_tts")
        self.assertEqual(result["status"], "MISMATCH")
        self.assertEqual(result["reason"], "required_model_missing_actual")

    def test_same_catalog_and_size_but_different_payload_is_mismatch(self):
        expected = pack([("required", [("model", b"123456")])])
        actual = pack([("required", [("model", b"abcdef")])])
        self.assertEqual(len(expected), len(actual))
        result = compare_images(expected, actual, "required")
        self.assertEqual(result["status"], "MISMATCH")
        self.assertEqual(result["reason"], "catalog_mismatch")

    def test_rejects_bad_model_count_and_truncated_header(self):
        with self.assertRaisesRegex(ImageError, "model_count_limit"):
            parse_image(struct.pack("<I", 257))
        with self.assertRaisesRegex(ImageError, "truncated_header"):
            parse_image(struct.pack("<I", 1) + b"short")

    def test_rejects_file_count_limit(self):
        # One complete model header with an unreasonable count, no file entries.
        data = struct.pack("<I", 1) + b"wake".ljust(32, b"\0") + struct.pack("<I", 4097)
        with self.assertRaisesRegex(ImageError, "file_count_limit"):
            parse_image(data)

    def test_rejects_header_offset_and_out_of_bounds_length(self):
        image = bytearray(pack([("required", [("model", b"data")])]))
        struct.pack_into("<I", image, 4 + 32 + 4 + 32, 0)
        with self.assertRaisesRegex(ImageError, "payload_before_header_end"):
            parse_image(bytes(image))

        image = bytearray(pack([("required", [("model", b"data")])]))
        struct.pack_into("<I", image, 4 + 32 + 4 + 32 + 4, 99)
        with self.assertRaisesRegex(ImageError, "payload_out_of_bounds"):
            parse_image(bytes(image))

    def test_rejects_overlapping_payload_ranges(self):
        image = bytearray(pack([("required", [("one", b"abcd"), ("two", b"efgh")])]))
        second_offset = 4 + 36 + 40 + 32
        struct.pack_into("<I", image, second_offset, struct.unpack_from("<I", image, second_offset)[0] - 1)
        with self.assertRaisesRegex(ImageError, "overlapping_payloads"):
            parse_image(bytes(image))

    def test_rejects_duplicate_model_and_file_names(self):
        duplicate_models = pack([("same", []), ("same", [])])
        with self.assertRaisesRegex(ImageError, "duplicate_model_name"):
            parse_image(duplicate_models)
        duplicate_files = pack([("required", [("same", b"a"), ("same", b"b")])])
        with self.assertRaisesRegex(ImageError, "duplicate_file_name"):
            parse_image(duplicate_files)

    def test_rejects_non_ascii_names_and_oversized_input(self):
        image = bytearray(pack([("required", [])]))
        image[4] = 0xff
        with self.assertRaisesRegex(ImageError, "non_ascii_name"):
            parse_image(bytes(image))
        with self.assertRaisesRegex(ImageError, "input_too_large"):
            parse_image(pack([("required", [])]) + b"x", max_input_bytes=4)


if __name__ == "__main__":
    unittest.main()
