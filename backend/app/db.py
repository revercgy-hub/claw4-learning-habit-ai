# claw4/backend/app/db.py
# SQLAlchemy engine/session wiring for the persistent family backend
# (WB-STREAM-002 CP5).
#
# The database dialect NEVER leaks into business code: repository methods only
# use the SQLAlchemy ORM API, so tests run on SQLite while deployment points
# at PostgreSQL via CLAW4_DATABASE_URL. No real database is ever contacted by
# this repo's tests; the default in-memory SQLite is a single shared
# connection (StaticPool) so every request/thread sees the same schema.
from __future__ import annotations

import os

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from app import models

__all__ = ["configure_database", "get_db", "engine", "SessionLocal", "create_schema"]

_DEFAULT_URL = "sqlite:///./.claw4_host_mvp.db"


def _make_engine(url: str):
    if url.startswith("sqlite"):
        # File-based SQLite with a queue pool: each thread gets its own
        # connection so the concurrency tests are safe; a generous busy
        # timeout absorbs SQLite's writer lock during short transactions.
        # (An in-memory StaticPool would serialize every thread onto one
        # shared connection and deadlock under concurrency.)
        return create_engine(
            url,
            connect_args={"check_same_thread": False, "timeout": 15},
            future=True,
        )
    return create_engine(url, pool_pre_ping=True, future=True)


engine = _make_engine(os.environ.get("CLAW4_DATABASE_URL", _DEFAULT_URL))
SessionLocal = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False,
                            future=True)


def configure_database(url: str) -> None:
    """(Re)binds the engine/session factory — used by tests and the app entry
    point when CLAW4_DATABASE_URL is set before import."""
    global engine, SessionLocal
    engine = _make_engine(url)
    SessionLocal = sessionmaker(bind=engine, autoflush=False,
                                expire_on_commit=False, future=True)


def create_schema() -> None:
    """Create all tables (idempotent). Development/test helper — production
    would run migrations; this repo only ships the ORM models + a schema
    bootstrap for the host MVP."""
    models.Base.metadata.create_all(bind=engine)


def get_db():
    """FastAPI dependency: yields one session per request and commits it after
    the response, rolling back on any error (atomic per-request semantics)."""
    session: Session = SessionLocal()
    try:
        yield session
        session.commit()
    except Exception:
        session.rollback()
        raise
    finally:
        session.close()
