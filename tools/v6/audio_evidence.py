"""Read-only audio evidence from SHA-bound, explicitly declared boot sessions.

Manifest: {"elf_sha256": "<64 hex>", "captures": [{"path": "...",
"sha256": "...", "session": "boot-01"}]}. Paths are relative to manifest.
Captures must be chronological and non-overlapping within each session. The
first capture must contain exactly one matching firmware boot anchor. Later
fragments must contain none. Session continuity is operator-attested, not proven
by uptime alone. Output contains aggregates, never raw audio or network names.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ANSI = re.compile(r'\x1b\[[0-9;]*m')
LINE = re.compile(r'^[IWEVD] \((\d+)\) (.*)$')


def summarize(manifest, base):
    elf = manifest['elf_sha256'].lower()
    if not re.fullmatch(r'[0-9a-f]{64}', elf):
        raise ValueError('Full ELF SHA256 required')
    sessions = {}
    channels = {str(ch): dict(windows=0, samples=0, clipped=0, peak=0,
                             rms_max=0, raw_peak=0, read_failures=0, tx_overlap_n=0,
                             tx_overlap_peak=0) for ch in range(2)}
    seen_hashes = set()
    for capture in manifest['captures']:
        data = (base / capture['path']).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != capture['sha256'] or digest in seen_hashes:
            raise ValueError('Capture hash mismatch or duplicate capture')
        seen_hashes.add(digest)
        lines = ANSI.sub('', data.decode('utf-8', errors='replace')).splitlines()
        anchors = re.findall(r'CANDIDATE_ELF_SHA256=([0-9a-fA-F]+)', '\n'.join(lines))
        sid = capture['session']
        new = sid not in sessions
        if new:
            if len(anchors) != 1 or len(anchors[0]) < 9 or not elf.startswith(anchors[0].lower()):
                raise ValueError('Session requires one matching boot anchor')
            sessions[sid] = dict(last=-1, wakes=set(), health=set(), rearms=0)
        elif anchors or any('Calling app_main()' in line or line.startswith('rst:') for line in lines):
            raise ValueError('Reset in continuation; declare a new session')
        state = sessions[sid]
        stamped = [(int(m[1]), m[2]) for line in lines if (m := LINE.match(line))]
        if not stamped or stamped[0][0] <= state['last']:
            raise ValueError('Missing timestamps or overlapping/out-of-order fragment')
        # First boot may contain bootloader/application timestamp resets; only
        # consume the application stream after the verified anchor.
        if new:
            start = next((i for i, (_, msg) in enumerate(stamped)
                          if 'CANDIDATE_ELF_SHA256=' in msg), None)
            if start is None:
                raise ValueError('Boot anchor must be timestamped')
            stamped = stamped[start:]
        for stamp, msg in stamped:
            if stamp < state['last']:
                raise ValueError('Uptime rollback within session')
            state['last'] = stamp
            wake = re.search(r'WAKE_DETECTED .*count=(\d+)', msg)
            if wake:
                count = int(wake[1])
                if state['wakes'] and count < max(state['wakes']):
                    raise ValueError('Wake counter rollback')
                state['wakes'].add(count)
            if 'V6M0: HEALTH ' in msg:
                state['health'].add(stamp)
            if 'V6M0: WAKE_REARM ' in msg:
                state['rearms'] += 1
            if 'Claw4Audio: INPUT ' in msg:
                fields = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', msg)}
                ch = channels[str(fields['ch'])]
                ch['windows'] += 1
                for target, source in [('samples', 'n'), ('clipped', 'clipped'), ('read_failures', 'read_failures'), ('tx_overlap_n', 'tx_overlap_n')]:
                    ch[target] += fields.get(source, 0)
                for target, source in [('peak', 'peak'), ('rms_max', 'rms'), ('raw_peak', 'raw_peak'), ('tx_overlap_peak', 'tx_overlap_peak')]:
                    ch[target] = max(ch[target], fields.get(source, 0))
    for ch in channels.values():
        ch['input_status'] = ('FAIL' if ch['clipped'] or ch['read_failures'] else
                              'OBSERVED' if ch['samples'] else 'NOT_VERIFIED')
    return dict(schema='claw4-v6-audio-evidence/1', sessions=len(sessions),
                captures=len(seen_hashes), elf_sha256=elf,
                wake_events_observed=sum(len(s['wakes']) for s in sessions.values()),
                health_samples_observed=sum(len(s['health']) for s in sessions.values()),
                rearm_lines_observed=sum(s['rearms'] for s in sessions.values()),
                channels=channels, reference_status='HARDWARE_VERIFY_REQUIRED',
                limits='Session continuity operator-attested. TX overlap is write-call overlap, '
                       'not physical DAC timing. Zero idle reference does not prove failure; '
                       'nonzero reference does not prove AEC quality. Counts cover captured events only.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = summarize(json.loads(args.manifest.read_text(encoding='utf-8-sig')), args.manifest.parent)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
