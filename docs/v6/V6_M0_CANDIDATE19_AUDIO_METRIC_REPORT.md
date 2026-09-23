# V6 M0 Candidate 19 — PCM input headroom telemetry

Date: 2026-09-23. Candidate19 corrects an audio diagnostic field that no longer measured anything after the full-width PCM normalization change. It adds a per-channel count of normalized PCM16 samples at or above the near-full-scale threshold; it does not tune AEC, gain, microphone settings, or playback timing.

## Implementation

- Replaces the never-incremented `clipped` counter with `near_full_scale_n`, counting samples where `abs(PCM16) >= 32760` in each one-second input window. The threshold leaves at most eight positive PCM16 counts of headroom.
- Keeps `peak` and `raw_peak`. The near-full-scale and peak values describe samples prepared for the AFE; `raw_peak` remains tied to the physical I2S receive slots, including the silent hardware reference slot.
- Updates `audio_evidence.py` to aggregate the new metric, retain old `clipped` fields with an explicit firmware-context warning, and mark near-full-scale data unavailable for old firmware captures. A near-full-scale count is a digital headroom observation, not proof of ADC or acoustic clipping and not an AEC result.
- Hardens `build_device.py` so a staged build first mirrors the authoritative Claw4 board overlay after checking file inventory. Missing or extra staged files fail before any copy, preventing a stale board build from being mistaken for a candidate.

## Build and test evidence

- Candidate manifest: [`integration/v6/m0-candidate-19.json`](../../integration/v6/m0-candidate-19.json).
- ESP-IDF 6.1 incremental reconfigure/build succeeded after all 11 board overlay files were hash-verified in the staged tree. The app image is 3,247,920 bytes, SHA256 `4aa6a4c8ef4eb23b005134cbdff4d99b6831ac0ac82df32eb41337f1119b80ed`; ELF SHA256 `09f21752f2eb2d05961cf71b826a47bc3ec7e8a2c067125f26743dcf9de1d919`. The app partition reports 78% free.
- Build log: `out/v6-device-private/candidate19-build-02.txt`, SHA256 `db77100a8f454bc6bcf833c5e7b10b142c459e6ad595ba62d0b259d882a87f79`.
- `py -3.14 -m unittest discover -s tools/v6 -p "test_*.py"`: 68 tests passed, including new log-compatibility and staged-overlay synchronization cases.
- `test_board_algorithms.cc` compiled with C++17 warnings enabled and passed its assertions. `freeze_candidate.py` verified the staged board inventory and source hashes. `git diff --check` passed.

## Device gate

Candidate19 has **not** been flashed. The PCM telemetry is build-verified only; no controlled input-level observation was collected. Candidate17's old `clipped=0` values are superseded for clipping interpretation. AEC remains **NOT PASS**; controlled playback, near-end wake-word, camera frame capture, device network failure injection, and longer stability checks remain open M0 items. No raw audio is stored.
