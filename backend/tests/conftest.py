# claw4/backend/tests/conftest.py
# pytest session bootstrap for the persistent backend (WB-STREAM-002 CP5).
#
# Each pytest session gets its OWN throwaway SQLite database under the system
# temp dir: CLAW4_DATABASE_URL is set BEFORE app.main is imported, so the
# engine/schema/seed are created against a fresh file. Nothing in this repo
# ever touches a real database/NAS, and no repository files are deleted.
from __future__ import annotations

import os
import tempfile

_tmpdir = tempfile.mkdtemp(prefix="claw4-pytest-")
_dbpath = os.path.join(_tmpdir, "test.db")
# Forward slashes keep the SQLAlchemy URL well-formed on Windows.
os.environ["CLAW4_DATABASE_URL"] = "sqlite:///" + _dbpath.replace(os.sep, "/")
