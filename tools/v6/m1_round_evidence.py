"""Fail-closed M1 voice-round evidence validator.

Input JSON schema (unknown fields are ignored):
{"schema":"claw4-v6-m1-rounds/1","session_id":"...",
 "candidate":{"reviewed_source_sha":"<40 hex>",
              "app_sha256":"<64 hex>","elf_sha256":"<64 hex>"},
 "server":{"repo":"...","commit":"<40 hex>","image":"name@sha256:<64 hex>"},
 "config_sha256":"<64 hex>","config_version":"optional label",
 "provider_versions":{"llm":"...","asr":"...","tts":"..."},
 "rounds":[{"round":1,"scenario":"short|long|silence|interruption",
 "session_id":"...","candidate":{...},"server":{...},
 "latencies_ms":{"asr_final":{"value":12,"clock":"service_monotonic"},
 "llm_first_token":{...},"tts_first_audio":{...},
 "audible_first_response":{"value":12,"clock":"device_monotonic"}},
 "resources":{"heap_min_bytes":123,"psram_min_bytes":null},
 "flags":{"misrecapture":false,"stuck":false,"crash":false,"wdt":false,
 "reboot":false,"looping_capture":false,"lost_response":false}}, ...],
 "fault_checks":{"nas_disconnect_recovery":"PASS|FAIL|NOT_VERIFIED", ...}}
Fault check keys: nas_disconnect_recovery, websocket_reconnect,
tts_midstream_network_loss, silence_timeout, wake_without_asr,
repeated_interruptions, self_capture_during_tts, self_capture_after_tts.
Null metrics mean explicitly unavailable and yield NOT_VERIFIED. The plan does
not define latency thresholds, resource thresholds, or scenario quotas; those
are reported without inferring success. Clock domains are preserved per metric.
No prompt, transcript, or secret is accepted into output.
"""
import argparse
import json
import math
import re
import statistics
from pathlib import Path

SCHEMA = 'claw4-v6-m1-rounds/1'
FAULT_CHECKS = ('nas_disconnect_recovery', 'websocket_reconnect',
                'tts_midstream_network_loss', 'silence_timeout',
                'wake_without_asr', 'repeated_interruptions',
                'self_capture_during_tts', 'self_capture_after_tts')
METRICS = ('asr_final', 'llm_first_token', 'tts_first_audio',
           'audible_first_response')
FLAGS = ('misrecapture', 'stuck', 'crash', 'wdt', 'reboot',
         'looping_capture', 'lost_response')
SCENARIOS = ('short', 'long', 'silence', 'interruption')


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


def _identity(obj):
    _require(isinstance(obj, dict), 'identity object missing')
    source = obj.get('reviewed_source_sha')
    app = obj.get('app_sha256'); elf = obj.get('elf_sha256')
    _require(isinstance(source, str) and re.fullmatch(r'[0-9a-fA-F]{40}', source),
             'full reviewed source SHA required')
    _require(isinstance(app, str) and re.fullmatch(r'[0-9a-fA-F]{64}', app), 'full app SHA256 required')
    _require(isinstance(elf, str) and re.fullmatch(r'[0-9a-fA-F]{64}', elf), 'full ELF SHA256 required')
    return {'reviewed_source_sha': source.lower(),
            'app_sha256': app.lower(), 'elf_sha256': elf.lower()}


def _server(obj):
    _require(isinstance(obj, dict), 'server identity missing')
    repo, commit, image = obj.get('repo'), obj.get('commit'), obj.get('image')
    _require(isinstance(repo, str) and repo.strip(), 'server repo required')
    _require(isinstance(commit, str) and re.fullmatch(r'[0-9a-fA-F]{40}', commit), 'full server commit SHA required')
    _require(isinstance(image, str) and re.search(r'@sha256:[0-9a-fA-F]{64}$', image), 'image must be pinned to full sha256 digest')
    return {'repo': repo, 'commit': commit.lower(), 'image': image.lower()}


def _versions(doc):
    cfg_sha = doc.get('config_sha256'); cfg_version = doc.get('config_version')
    providers = doc.get('provider_versions')
    _require(isinstance(cfg_sha, str) and re.fullmatch(r'[0-9a-fA-F]{64}', cfg_sha),
             'full config SHA256 required')
    if cfg_version is not None:
        _require(isinstance(cfg_version, str) and cfg_version.strip(), 'config version must be a nonempty label')
    _require(isinstance(providers, dict) and all(isinstance(providers.get(k), str) and providers[k].strip()
            for k in ('llm', 'asr', 'tts')), 'LLM/ASR/TTS provider versions required')
    result = {'config_sha256': cfg_sha.lower(),
              'provider_versions': {k: providers[k] for k in ('llm', 'asr', 'tts')}}
    if cfg_version is not None:
        result['config_version'] = cfg_version
    return result


def _pctl(values, p):
    ordered = sorted(values)
    if len(ordered) == 1: return ordered[0]
    rank = (len(ordered) - 1) * p
    lo = math.floor(rank); hi = math.ceil(rank)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (rank - lo)


def summarize(doc):
    _require(isinstance(doc, dict) and doc.get('schema') == SCHEMA, 'wrong schema')
    sid = doc.get('session_id')
    _require(isinstance(sid, str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._:-]{2,127}', sid), 'valid session_id required')
    candidate, server = _identity(doc.get('candidate')), _server(doc.get('server'))
    versions = _versions(doc)
    rounds = doc.get('rounds')
    _require(isinstance(rounds, list) and len(rounds) == 20, 'exactly 20 rounds required')
    values = {m: {} for m in METRICS}; domains = {m: set() for m in METRICS}
    unavailable = set(); coverage = {s: 0 for s in SCENARIOS}; reasons = []
    fail = False
    for index, row in enumerate(rounds, 1):
        _require(isinstance(row, dict) and type(row.get('round')) is int and row['round'] == index,
                 'rounds must be unique integer values ordered 1..20')
        _require(row.get('session_id') == sid, 'mixed or missing session identity')
        _require(_identity(row.get('candidate')) == candidate, 'mixed candidate identity')
        _require(_server(row.get('server')) == server, 'mixed server identity')
        scenario = row.get('scenario')
        _require(scenario in SCENARIOS, 'invalid or missing scenario')
        coverage[scenario] += 1
        _require(row.get('device_restarted') is False and row.get('service_restarted') is False
                 and row.get('manual_recovery') is False, 'restart or manual per-round recovery invalidates run')
        latency = row.get('latencies_ms'); resource = row.get('resources')
        _require(isinstance(latency, dict) and isinstance(resource, dict), 'latencies and resources required')
        for metric in METRICS:
            item = latency.get(metric)
            _require(isinstance(item, dict) and item.get('clock') in ('device_monotonic', 'service_monotonic'), f'{metric} value and clock required')
            value = item.get('value')
            if value is None:
                unavailable.add(metric)
                continue
            _require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value >= 0,
                     f'{metric} must be nonnegative finite or explicit null')
            values[metric].setdefault(item['clock'], []).append(float(value)); domains[metric].add(item['clock'])
        for key in ('heap_min_bytes', 'psram_min_bytes'):
            value = resource.get(key, 'MISSING')
            _require(value != 'MISSING', f'{key} required; use null for N/A')
            if value is None:
                unavailable.add(key)
            else:
                _require(type(value) is int and value >= 0,
                         f'{key} must be a nonnegative integer or explicit null')
        flags = row.get('flags')
        _require(isinstance(flags, dict) and all(type(flags.get(k)) is bool for k in FLAGS), 'all critical flags must be recorded as booleans')
        for key in FLAGS:
            if flags[key]:
                fail = True; reasons.append(f'round {index}: {key}')
    checks = doc.get('fault_checks')
    _require(isinstance(checks, dict), 'fault checks required')
    check_states = {k: checks.get(k) if checks.get(k) in ('PASS', 'FAIL', 'NOT_VERIFIED') else 'NOT_VERIFIED'
                    for k in FAULT_CHECKS}
    failed_checks = [k for k, state in check_states.items() if state == 'FAIL']
    missing_checks = [k for k, state in check_states.items() if state == 'NOT_VERIFIED']
    reasons.extend(f'fault check failed: {k}' for k in failed_checks)
    reasons.extend(f'fault check not verified: {k}' for k in missing_checks)
    metrics = {}
    for metric in METRICS:
        by_clock = {clock: {'n': len(vals), 'p50_ms': _pctl(vals, .5), 'p95_ms': _pctl(vals, .95),
                            'max_ms': max(vals)} for clock, vals in values[metric].items()}
        vals = next(iter(values[metric].values())) if len(values[metric]) == 1 else []
        metrics[metric] = {'n': sum(len(v) for v in values[metric].values()), 'clock_domains': sorted(domains[metric]),
                           'status': 'NOT_VERIFIED' if metric in unavailable else 'OBSERVED',
                           'p50_ms': _pctl(vals, .5) if vals else None,
                           'p95_ms': _pctl(vals, .95) if vals else None,
                           'max_ms': max(vals) if vals else None,
                           'by_clock_domain': by_clock}
    for key in ('heap_min_bytes', 'psram_min_bytes'):
        vals = [row['resources'][key] for row in rounds if row['resources'][key] is not None]
        metrics[key] = {'n': len(vals), 'status': 'NOT_VERIFIED' if key in unavailable else 'OBSERVED',
                        'min': min(vals) if vals else None}
    required_coverage = all(coverage[s] > 0 for s in SCENARIOS)
    reasons.extend(f'scenario not covered: {s}' for s in SCENARIOS if coverage[s] == 0)
    status = 'FAIL' if fail or failed_checks else ('NOT_VERIFIED' if missing_checks or unavailable or not required_coverage else 'PASS')
    if fail: reasons.insert(0, 'critical failure flag observed')
    if not reasons and status == 'PASS': reasons.append('20-round continuity and recorded checks satisfy declared evidence rules; no performance thresholds are defined')
    return {'schema': 'claw4-v6-m1-round-report/1', 'status': status, 'session_id': sid,
            'candidate': candidate, 'server': server, **versions, 'round_count': len(rounds),
            'scenario_coverage': coverage, 'metrics': metrics,
            'fault_checks': check_states, 'reasons': reasons,
            'limits': 'No latency/resource pass thresholds or scenario quotas are defined. PASS means evidence structure and recorded checks only; it does not establish voice quality. Timing aggregates are per metric and retain source monotonic clock domain; no cross-host timing is derived.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(summarize(json.loads(args.input.read_text(encoding='utf-8'))), indent=2), encoding='utf-8')
