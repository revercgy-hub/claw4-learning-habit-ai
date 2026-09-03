# claw4/backend/app/clock.py
# Injectable clock / timezone helpers (WB-STREAM-002 CP5).
#
# Business code must never hardcode dates like "2026-09-02". Tests monkeypatch
# the functions in this module to make time deterministic and to exercise
# timezone boundaries. The default timezone for "local date" decisions is read
# from CLAW4_TZ (IANA name, default UTC) so the same code can be deployed for
# any family timezone without code changes.
from __future__ import annotations

import os
import time
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

__all__ = [
    "utc_now_epoch",
    "utc_now_iso",
    "local_today",
    "local_date_of",
    "local_day_epoch_bounds",
    "tz_name",
]

_TZ_CACHE: dict[str, ZoneInfo] = {}


def tz_name() -> str:
    return os.environ.get("CLAW4_TZ", "UTC").strip() or "UTC"


def _zone() -> ZoneInfo:
    name = tz_name()
    z = _TZ_CACHE.get(name)
    if z is None:
        z = ZoneInfo(name)
        _TZ_CACHE[name] = z
    return z


def utc_now_epoch() -> float:
    """Seconds since the epoch (UTC), float; injectable for tests."""
    return time.time()


def utc_now_iso() -> str:
    return datetime.fromtimestamp(utc_now_epoch(), tz=timezone.utc).isoformat()


def local_date_of(epoch: float | None = None) -> str:
    """ISO date (YYYY-MM-DD) in the configured local timezone."""
    ts = utc_now_epoch() if epoch is None else epoch
    return datetime.fromtimestamp(ts, tz=_zone()).strftime("%Y-%m-%d")


def local_today() -> str:
    return local_date_of()


def local_day_epoch_bounds(local_date: str) -> tuple[float, float]:
    """UTC epoch bounds [start, end) of one LOCAL calendar day.

    Accepts an ISO date 'YYYY-MM-DD' (the same format the dashboard / task
    scheduled_date use) and returns the inclusive start and exclusive end of
    that day in the configured family timezone, expressed as unix epoch
    seconds. It is the ONLY sanctioned way to convert a local date into a UTC
    time range — business code must never hand-roll 86400-second math, which
    breaks across DST and non-UTC deployments.
    """
    try:
        day = datetime.strptime(local_date, "%Y-%m-%d").replace(tzinfo=_zone())
    except ValueError as exc:
        raise ValueError(f"invalid local date: {local_date!r}") from exc
    # Add one calendar day in the local zone (timedelta arithmetic on an
    # aware datetime keeps DST transitions correct), then convert to epoch.
    return day.timestamp(), (day + timedelta(days=1)).timestamp()
