import json

import pytest

from app.deployment import load_credentials


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
