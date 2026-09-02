# claw4/backend/tests/conftest.py
# pytest session bootstrap for the persistent backend (WB-STREAM-002 CP5).
#
# The host MVP database is a local SQLite file. Each pytest session starts
# from a clean database: the file is removed here (conftest is imported
# before any test module, hence before app.main creates the engine/schema and
# seeds the development-session stub data). Nothing in this repo ever touches
# a real database/NAS.
from __future__ import annotations

import pathlib

_DB = pathlib.Path(__file__).resolve().parent.parent / ".claw4_host_mvp.db"
if _DB.exists():
    try:
        _DB.unlink()
    except PermissionError:
        pass  # engine still open from a previous in-process session; tests
        # that need a truly fresh store should use a unique CLAW4_DATABASE_URL.
