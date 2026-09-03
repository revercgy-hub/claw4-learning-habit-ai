# claw4/backend/tests/test_clock.py
# Unit tests for clock.local_day_epoch_bounds (FIX-03).
#
# The helper is the ONLY sanctioned way to convert a local ISO date into a UTC
# epoch range; business code must never hand-roll 86400-second math (it breaks
# across DST and non-UTC deployments). These tests pin the exact epoch math in
# UTC, a fixed-offset zone (Asia/Shanghai) and a DST transition day
# (America/New_York 2026-03-08, a 23-hour local day).
from __future__ import annotations

import pytest

from app import clock


@pytest.fixture(autouse=True)
def _clear_tz_cache():
    clock._TZ_CACHE.clear()
    yield
    clock._TZ_CACHE.clear()


def test_utc_bounds_exact():
    # Default CLAW4_TZ is UTC: a full local day is exactly 86400 epoch seconds
    # starting at the UTC midnight of the requested ISO date.
    start, end = clock.local_day_epoch_bounds("1970-01-02")
    assert (start, end) == (86400.0, 172800.0)


def test_bounds_roundtrip_local_date():
    # Every epoch second inside [start, end) maps back to the requested date.
    start, end = clock.local_day_epoch_bounds("2026-09-02")
    assert clock.local_date_of(start) == "2026-09-02"
    assert clock.local_date_of(end - 1) == "2026-09-02"
    assert clock.local_date_of(end) == "2026-09-03"  # exclusive end


def test_shanghai_fixed_offset_bounds(monkeypatch):
    # Asia/Shanghai is UTC+8 with no DST: the local midnight of 1970-01-02 is
    # 16:00 UTC on 1970-01-01; the day is still 86400 seconds long.
    monkeypatch.setenv("CLAW4_TZ", "Asia/Shanghai")
    start, end = clock.local_day_epoch_bounds("1970-01-02")
    assert start == 16 * 3600
    assert end - start == 86400


def test_dst_transition_day_length(monkeypatch):
    # 2026-03-08 is the US spring-forward day in America/New_York: the local
    # day is 23 hours long. Hand-rolled 86400 math would be wrong here.
    monkeypatch.setenv("CLAW4_TZ", "America/New_York")
    start, end = clock.local_day_epoch_bounds("2026-03-08")
    assert end - start == 23 * 3600
    assert clock.local_date_of(start) == "2026-03-08"
    assert clock.local_date_of(end - 1) == "2026-03-08"


def test_invalid_date_rejected():
    with pytest.raises(ValueError):
        clock.local_day_epoch_bounds("2026-13-01")
    with pytest.raises(ValueError):
        clock.local_day_epoch_bounds("not-a-date")
