import json

import pytest

from app.deployment import load_credentials
from app.lan_validation import app
from fastapi.testclient import TestClient


def test_deployment_requires_distinct_strong_values(tmp_path):
    path = tmp_path / "credentials.json"
    with pytest.raises(RuntimeError, match="Invalid Claw4 backend credential file"):
        load_credentials(str(path))
    keys = {"device_token_key": "a1" * 32, "parent_1": "b2" * 32, "parent_2": "c3" * 32}
    path.write_text(json.dumps(keys), encoding="utf-8")
    assert load_credentials(str(path)) == tuple(keys.values())
    keys["parent_2"] = keys["parent_1"]
    path.write_text(json.dumps(keys), encoding="utf-8")
    with pytest.raises(RuntimeError, match="Invalid Claw4 backend credential file"):
        load_credentials(str(path))


def test_validation_lan_exposes_device_routes_only():
    with TestClient(app) as client:
        assert client.get("/health").json()["profile"] == "device-validation"
        assert client.post("/api/v1/devices/challenge", json={}).status_code == 422
        assert client.get("/api/v1/children/child-1/tasks/today").status_code == 401
        assert client.post("/api/v1/events/batch", json={}).status_code == 422
        assert client.post("/api/v1/devices/register", json={}).status_code == 404
        assert client.post("/api/v1/devices/claim", json={}).status_code == 404
        assert client.get("/api/v1/parents/me").status_code == 404
        assert client.get("/openapi.json").status_code == 404
