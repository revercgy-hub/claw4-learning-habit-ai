import unittest
from repeat_boot import classify


class BootSeriesTests(unittest.TestCase):
    good = 'Calling app_main()\nCANDIDATE_ELF_SHA256=abcdef123\nV6M0: BOOT_READY\n'

    def test_single_matching_boot(self):
        self.assertTrue(classify(self.good, 'abcdef123456')['passed'])

    def test_success_after_crash_is_failure(self):
        self.assertFalse(classify('abort() was called\n' + self.good, 'abcdef123456')['passed'])

    def test_missing_ready_or_wrong_firmware_is_not_success(self):
        self.assertFalse(classify(self.good, 'bbbbbbbbbb')['passed'])
        self.assertFalse(classify(self.good.replace('V6M0: BOOT_READY', ''), 'abcdef123456')['passed'])

    def test_two_boots_cannot_pass_one_round(self):
        self.assertFalse(classify(self.good * 2, 'abcdef123456')['passed'])
