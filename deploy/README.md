# Claw4 Host-MVP deployment notes (CP5).
#
# Scope: static configuration only. The current host has no Docker daemon, so
# nothing here has been executed; `docker compose config` (or any YAML
# parser) is the only validation performed. Containers are intentionally NOT
# claimed to have run.

## Dialect isolation
- All persistence code lives behind the SQLAlchemy ORM (`backend/app/models.py`)
  and the repository (`backend/app/store.py`). No SQL dialect leaks into the
  FastAPI business layer (`backend/app/main.py`).
- Tests run on a local SQLite file (`backend/.claw4_host_mvp.db`, git-ignored,
  recreated by `tests/conftest.py` every pytest session).
- Deployment targets PostgreSQL 16 via `docker-compose.yml` +
  `backend.Dockerfile` (psycopg[binary] driver added at image build time).

## Environment
- `.env.example` documents `CLAW4_DATABASE_URL` and `CLAW4_TZ`. No secrets,
  keys or real family/child data are ever stored by this repository.
- Parent identity is a development-session stub token (single family), never
  a production login.

## Migrations
- `db.create_schema()` creates tables from the ORM metadata (host-MVP
  bootstrap). Production would replace this with a migration tool (e.g.
  Alembic); that is out of scope for CP5.

## Static validation performed by the test suite
- `python -c "import yaml; yaml.safe_load(open('docker-compose.yml'))"`
  (PyYAML is already a locked transitive dependency).
