import hashlib
import struct
import unittest
from flash_plan import partitions, validate_layout, validate_writes


class FlashPlanTests(unittest.TestCase):
    def test_table_checksum(self):
        entry = struct.pack('<HBBII16sI', 0x50aa, 0, 0, 0x200000, 14*1024*1024, b'factory', 0)
        table = entry + b'\xeb\xeb' + b'\xff'*14 + hashlib.md5(entry).digest()
        self.assertEqual(partitions(table)[0][0], 'factory')
        with self.assertRaises(ValueError):
            partitions(table[:-1] + bytes([table[-1] ^ 1]))

    def test_blank_table_rejected(self):
        with self.assertRaises(ValueError):
            partitions(b'\xff'*4096)

    def test_allowed_writes(self):
        validate_writes([(0x2000, 20000), (0x9000, 3072), (0x10f000, 900000), (0x200000, 10000000), (0x1000000, 5000000)])

    def test_nvs_write_rejected(self):
        with self.assertRaises(ValueError):
            validate_writes([(0xa000, 4096)])

    def test_bootloader_crossing_table_rejected(self):
        with self.assertRaises(ValueError):
            validate_writes([(0x2000, 30000), (0x9000, 3072), (0x10f000, 10), (0x200000, 10), (0x1000000, 10)])

    def test_model_crossing_app_rejected(self):
        with self.assertRaises(ValueError):
            validate_writes([(0x2000, 20000), (0x9000, 3072), (0x10f000, 1000000), (0x200000, 10), (0x1000000, 10)])

    def test_rename_only(self):
        old = [('factory', 0, 0, 0x200000, 14*1024*1024, 0),
               ('resources', 1, 0x82, 0x1000000, 15*1024*1024, 0)]
        new = [old[0], ('assets', *old[1][1:])]
        validate_layout(old, new)
        changed = [old[0], ('assets', 1, 0x82, 0x1100000, 14*1024*1024, 0)]
        with self.assertRaises(ValueError):
            validate_layout(old, changed)


if __name__ == '__main__':
    unittest.main()
