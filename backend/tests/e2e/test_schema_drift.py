# claw4/backend/tests/e2e/test_schema_drift.py
# PWA/backend schema-drift guard (WB-STREAM-002 CP7 req. 7).
#
# The parent PWA's API client must speak exactly the contract the backend
# serves. This test reads the live FastAPI OpenAPI document (in-process) and
# asserts that EVERY /api/v1 path referenced by the frontend client exists in
# the backend, and that the frontend types mirror key response fields. Any
# drift between frontend and backend fails this test automatically.
from __future__ import annotations

import re
from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)

FRONTEND = Path(__file__).resolve().parents[3] / "frontend"
CLIENT_TS = FRONTEND / "src" / "api" / "client.ts"
TYPES_TS = FRONTEND / "src" / "api" / "types.ts"


def test_frontend_client_paths_exist_in_backend_openapi():
    assert CLIENT_TS.exists(), f"missing {CLIENT_TS}"
    openapi = client.get("/openapi.json").json()
    backend_paths = set(openapi["paths"].keys())
    source = CLIENT_TS.read_text(encoding="utf-8")
    # Normalize JS template segments: ${encodeURIComponent(childId)} -> {child_id}
    clean = re.sub(r"\$\{encodeURIComponent\((\w+)\)\}", r"{\1}", source)
    used_paths = set(re.findall(r"/api/v1/[\w/{}.-]+", clean))
    normalized = {p.replace("{childId}", "{child_id}").replace("{taskId}", "{task_id}")
                  for p in used_paths}
    missing = sorted(normalized - backend_paths)
    assert not missing, f"frontend calls backend paths missing from OpenAPI: {missing}"


def test_frontend_types_mirror_key_backend_fields():
    assert TYPES_TS.exists(), f"missing {TYPES_TS}"
    types = TYPES_TS.read_text(encoding="utf-8")
    # Dashboard payload fields served by backend schemas.py
    for field in ("planned_tasks", "completed_tasks", "completion_rate",
                  "focus_minutes", "current_activity"):
        assert field in types, f"frontend types missing dashboard field {field}"
    for field in ("actual_seconds", "pause_count", "xp", "finished_at"):
        assert field in types, f"frontend types missing study-session field {field}"
    for field in ("battery_percent", "last_sync_at", "last_acked_sequence",
                  "fw_version", "online"):
        assert field in types, f"frontend types missing device field {field}"
    # battery stays nullable -> displayed as 未知 (never fabricated)
    assert "battery_percent: number | null" in types
