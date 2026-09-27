# S-15 M1 WakeNet control diagnostic Candidate

**Task:** `S-15-M1-WAKENET-CONTROL-CANDIDATE`

**Source checkpoint:** `codex/v6-m1-wakenet-control-candidate-s15` from `7f7357da2e1299b150a17b979fb1a307d027441e`

**Candidate:** `claw4-learning-v6-m1-wnctl-s15-20260927-01`

**Status:** `BUILD_COMPLETE_REVIEW_REQUIRED / DEVICE_NOT_TESTED`

This fresh Candidate was staged at `E:/v6/m1-wakenet-ctl-s15` from pinned XiaoZhi `ac6deed3d8e75348475364bf40ad953c6cd48054` and built with pinned ESP-IDF `fff9895c82d744c7237be8847347bdd1b07c6643`. The exact source checkpoint was clean before staging. The opt-in M1 AFE diagnostic patch contains the R-08-reviewed four-field `M1_AFE_CTL` record: cumulative WakeNet enable/disable calls and their latest raw integer return codes. It does not claim that a logical wake request was applied to a particular AFE event-bit snapshot. The M1 fixed NAS endpoint, fail-closed NVS initialization, reviewed local Wi-Fi component guard, and live-layout partition inputs were retained. No product source, public interface, or architecture file changed for S-15.

The isolated build exited 0. Its final log contains three project-build success markers, with `afe_audio_engine.cc` compiled in both the initial and local-override passes. The final application is **3,137,392 bytes**, below the smallest 4 MiB application partition. Application SHA-256 is **`64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a`**; ELF SHA-256 is **`33bcb2d01031a24814489aef84eaf155b30a37546f68534583b3aea90f7d5769`**.

## Frozen identity

The private Candidate manifest is `E:/v6/m1-wakenet-ctl-s15-candidate.json`, SHA-256 **`0aa6738af1f1adf477b95cb65e92efd49c2d63dedf7547893f9ced022e175a6b`**. It binds the Candidate ID, source Git commit and 18 normalized Git-blob proofs, staged board inventory, exact reconstruction of both patched AFE files from pinned upstream anchors, final configuration and component lock, 23-file local Wi-Fi component inventory, live partition inputs, build log, and artifacts. It explicitly records `DEVICE_NOT_TESTED`, `flash_performed=false`, and no inherited device results.

| Evidence | SHA-256 |
| --- | --- |
| Stage manifest (`v6-stage-manifest.json`) | `d95ab8496f5db5004d77c24aee0f40cc06ef9afc9b7167d9d8c4b949789dc717` |
| Staged AFE implementation / header | `1207713347812768091be7572eaf55cc58e147c856099c5c4dffae6a5b48d8a5` / `e1593fe53d0025dd96de3156e7bd94c9a03038bd4bd98324b9ef356cf095ade8` |
| Final `sdkconfig` | `56d0f7239a7e3c5ccb688293c5f1aea3dff957daade95baf8c2569bdc8b0d712` |
| Seed / final `dependencies.lock` | `c811bfe049e3c99fa129fa3050d4807955db7d0aa90e6e97a42e84034f158a39` / `18efb5df229b4e429b9a2119a4b069346ef4a7d5ea180fdfcee97ac1c87661a2` |
| Live-layout CSV / generated partition-table binary | `14c937dd21618f5878ad48633d52c3b8473170bb28b30b353d2d64cdb4640d49` / `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f` |
| Final build log (`E:/v6/m1-wakenet-ctl-s15-build.log`) | `48d9ba98e24b2accb637f4805fc857a49c3d2d4b9d5ab45cc41c7e83b2f23a27` |
| Application / ELF | `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a` / `33bcb2d01031a24814489aef84eaf155b30a37546f68534583b3aea90f7d5769` |

The final `sdkconfig` enables M1 input diagnostics, device AEC, AFE WakeNet, the selected `wn9_nihaoxiaozhi_tts` model, fixed NAS OTA URL, and custom `partitions/claw4-live-m1.csv`; M0 diagnostics are disabled. The generated 3,072-byte partition table matches the previously recorded live-layout binary hash `ef0039b636…`. The upstream build message about an absent assets partition did not change this final partition selection. This is a build-input comparison, not a new device read or partition write. The final component lock selects the reviewed local Wi-Fi override, its fail-closed NVS path passes the source guard, and the compile database contains AFE, Claw4 audio, and local Wi-Fi sources.

## Reproduction and limits

Staging used `py -3.14 -B tools/v6/stage_device.py --upstream E:/workbuddy/claw4-v6/vendor/xiaozhi-esp32 --destination E:/v6/m1-wakenet-ctl-s15 --variant m1 --m1-input-diagnostics`. Building used the pinned Python 3.12 interpreter at `E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe` with `tools/v6/build_device.py --source E:/v6/m1-wakenet-ctl-s15 --variant m1 --idf E:/workbuddy/claw4-v6/vendor/esp-idf --tools E:/workbuddy/claw4-v6/toolchains/idf61`. The V6 Host suite passed **148 tests, 0 failures/errors**; JSON, source-blob, staged-patch, component-lock, partition, artifact, and log-hash assertions passed; `git diff --check` passed.

No COM7, device, Flash, NVS, NAS, or shared `E:/v6/s1` operation occurred. Earlier S-11 and other DEVICE observations are historical and do **not** transfer to this Candidate. WakeNet control return codes, acoustic wake recovery, WebSocket Voice, AEC effectiveness, and M1 Preflight remain `HARDWARE_VERIFY_REQUIRED / NOT_VERIFIED`. Independent source/build review is required before any hardware handoff; this build does not establish M1 PASS or change the M0 gate.
