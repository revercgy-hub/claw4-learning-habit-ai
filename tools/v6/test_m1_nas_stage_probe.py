from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).with_name("m1_nas_stage_probe.py")
spec = importlib.util.spec_from_file_location("m1_nas_stage_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(probe)


class StageProbeTests(unittest.TestCase):
    def test_question_is_fixed_and_trial_bounds(self):
        self.assertEqual(probe.QUESTION, "What is 17 plus 25? Reply with one short sentence.")
        self.assertEqual(probe.MODEL, "qwen3.5:2b")
        self.assertEqual(probe.TTS_TYPE, "edge")

    def test_source_mismatch_fails_before_provider_import_or_request(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); prompt=root/"prompt"; prompt.write_text("private prompt")
            for rel in ("core/providers/llm/ollama/ollama.py","core/providers/tts/edge.py","core/providers/tts/base.py"):
                p=root/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_text("wrong")
            result=probe._worker(root,prompt)
        self.assertEqual(result["status"],"SOURCE_MISMATCH")
        self.assertNotIn("private prompt",json.dumps(result))

    def test_timeout_is_a_terminal_trial_and_output_is_metadata_only(self):
        with patch.object(probe.subprocess,"run",side_effect=subprocess.TimeoutExpired("worker",45)) as run:
            rows=probe.run_trials((Path("/upstream"),Path("/prompt")),2,45)
        self.assertEqual([r["status"] for r in rows],["TIMEOUT","TIMEOUT"])
        self.assertEqual(run.call_count,2)

    def test_malformed_or_truncated_worker_output_rejected(self):
        bad=subprocess.CompletedProcess([],0,stdout='{"status":"OK"',stderr="secret")
        with patch.object(probe.subprocess,"run",return_value=bad):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows,[{"trial":1,"status":"INVALID_WORKER_OUTPUT"}])
        self.assertNotIn("secret",json.dumps(rows))

    def test_nonzero_exit_rejected_even_with_success_json(self):
        good=subprocess.CompletedProcess([],1,stdout='{"status":"OK"}',stderr="private")
        with patch.object(probe.subprocess,"run",return_value=good):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows[0]["status"],"INVALID_WORKER_OUTPUT")


if __name__ == "__main__":
    unittest.main()
