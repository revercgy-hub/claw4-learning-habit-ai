import importlib.util
from pathlib import Path
import unittest


MODULE_PATH = (
    Path(__file__).resolve().parents[2]
    / "deploy"
    / "v6"
    / "xiaozhi-nas"
    / "claw4_echo_guard.py"
)
SPEC = importlib.util.spec_from_file_location("claw4_echo_guard", MODULE_PATH)
echo_guard = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(echo_guard)


class Conn:
    def __init__(self, sentence_id=None):
        self.sentence_id = sentence_id


class EchoGuardTests(unittest.TestCase):
    def test_exact_recent_echo_normalizes_punctuation_and_fullwidth(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "请现在开始写作业", now=10.0)
        echo_guard.record_packet(conn, "s1", now=10.1)
        echo_guard.record_packet(conn, "s1", now=12.0)
        snapshot = echo_guard.snapshot_echoes(conn, 12.393, 73_376)  # 2.293 s PCM window
        self.assertTrue(echo_guard.matches_echo("请，现在开始写作业！", snapshot))
        self.assertTrue(echo_guard.matches_echo("请现在开始写作业", snapshot))

    def test_distinct_text_and_substrings_do_not_match(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "请现在开始写作业", now=10)
        echo_guard.record_packet(conn, "s1", now=10.1)
        echo_guard.record_packet(conn, "s1", now=11)
        snap = echo_guard.snapshot_echoes(conn, 11.5, 16_000)
        self.assertFalse(echo_guard.matches_echo("请现在开始做作业", snap))
        self.assertFalse(echo_guard.matches_echo("现在开始写作业", snap))
        self.assertFalse(echo_guard.matches_echo("请现在开始写作业然后休息", snap))

    def test_requires_sent_packet_matching_connection_and_sentence(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "这是一句足够长度的话", now=10)
        self.assertFalse(echo_guard.snapshot_echoes(conn, 10.5, 16_000))
        echo_guard.record_packet(conn, "other", now=10.1)
        self.assertFalse(echo_guard.snapshot_echoes(conn, 10.5, 16_000))
        echo_guard.record_packet(conn, "s1", now=10.2)
        self.assertFalse(echo_guard.snapshot_echoes(Conn("s1"), 10.5, 16_000))
        conn.sentence_id = "s2"
        self.assertFalse(echo_guard.snapshot_echoes(conn, 10.5, 16_000))

    def test_overlap_expiry_and_invalid_capture(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "这是一句足够长度的话", now=10)
        echo_guard.record_packet(conn, "s1", now=10)
        self.assertFalse(echo_guard.snapshot_echoes(conn, 10.7, 3_200))  # after send tail
        self.assertFalse(echo_guard.snapshot_echoes(conn, 13.001, 16_000))  # expired
        for end, size in ((10.1, 0), (10.1, 3), (10.1, 96_002), (float("nan"), 16_000)):
            self.assertFalse(echo_guard.snapshot_echoes(conn, end, size))

    def test_snapshot_is_immutable_and_later_output_is_not_added(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "这是一句足够长度的话", now=10)
        echo_guard.record_packet(conn, "s1", now=10.1)
        echo_guard.record_packet(conn, "s1", now=10.5)
        snap = echo_guard.snapshot_echoes(conn, 10.6, 16_000)
        echo_guard.record_text(conn, "s2", "另外一句同样足够长的话", now=10.7)
        echo_guard.record_packet(conn, "s2", now=10.8)
        self.assertFalse(echo_guard.matches_echo("另外一句同样足够长的话", snap))
        self.assertTrue(echo_guard.matches_echo("这是一句足够长度的话", snap))

    def test_bad_new_text_clears_packet_association_and_finish_stops_packets(self):
        conn = Conn("s1")
        echo_guard.record_text(conn, "s1", "这是一句足够长度的话", now=10)
        echo_guard.record_packet(conn, "s1", now=10.1)
        echo_guard.record_text(conn, "s2", "短句", now=10.2)
        echo_guard.record_packet(conn, "s1", now=10.3)
        conn.sentence_id = "s2"
        self.assertFalse(echo_guard.snapshot_echoes(conn, 10.4, 16_000))

        conn.sentence_id = "s3"
        echo_guard.record_text(conn, "s3", "另一句够长度且可以匹配", now=11)
        echo_guard.record_packet(conn, "s3", now=11.1)
        echo_guard.finish_text(conn, "s3")
        echo_guard.record_packet(conn, "s3", now=11.2)
        self.assertTrue(
            echo_guard.matches_echo(
                "另一句够长度且可以匹配",
                echo_guard.snapshot_echoes(conn, 11.3, 16_000),
            )
        )


if __name__ == "__main__":
    unittest.main()
