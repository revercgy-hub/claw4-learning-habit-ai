# S-11 M1 AFE runtime diagnostic Candidate

**Task:** `S-11-M1-AFE-DIAG-CANDIDATE`
**Source branch/checkpoint:** `codex/v6-m1-afe-diag-s11` at `cc1af698ed0d13066c71ae56d9365acb220683b7`
**Candidate:** `claw4-learning-v6-m1-afe-diag-s11-20260927-01`
**Status:** `BUILD_COMPLETE_REVIEW_REQUIRED / DEVICE_NOT_TESTED`

The new Candidate was staged in `E:/v6/m1-afe-diag-s11` from the pinned XiaoZhi commit `ac6deed3d8e75348475364bf40ad953c6cd48054`. It uses the reviewed S-10 AFE aggregate counters, fixed M1 NAS endpoint policy, fail-closed NVS initialization, the live-layout partition CSV, and the pinned ESP-IDF commit `fff9895c82d744c7237be8847347bdd1b07c6643`. The source checkout was clean at the exact integration checkpoint before staging and manifest freeze. No product source or public interface was changed for S-11.

The final isolated build exited 0 after the reviewed local Wi-Fi NVS guard was selected and rebuilt. The log has three project-build success markers; its final segment completes the guarded reconfigure/build. Both the initial and final passes compiled `afe_audio_engine.cc`. The final app is **3,137,328 bytes**, SHA-256 `5038ed6e5649e8d11794dcdfbffae30d0a2e89cecceb182e00d424924da00a74`; the ELF is **48,049,520 bytes**, SHA-256 `6a050109fdb821e7464ae9418bb6d137053ddbc550fa9b632cf093e69b57660a`. The app fits the smallest 4 MiB application partition with 25% reported free.

## Frozen identity

The private immutable evidence manifest is `E:/v6/m1-afe-diag-s11-candidate.json`, SHA-256 **`4c973902c3b7ba4cd031c1471868c71b6e9bb172fa5e456fef503fa8cf834ba0`**. It binds the Candidate ID, source Git SHA and 18 normalized Git-blob proofs, staged overlay inventory, exact reconstruction of both patched AFE files from pinned upstream anchors, final configuration and component lock, local Wi-Fi component inventory, partition inputs, build log, and artifacts. Its `device_validation` is `DEVICE_NOT_TESTED` and `flash_performed` is `false`.

| Evidence | SHA-256 |
| --- | --- |
| Stage manifest (`v6-stage-manifest.json`) | `ea4853c201a3a0294660bc211684deaf9efc1415962757ba7726c36240bf1ce8` |
| Staged AFE implementation / header | `11b3967b0c15611df6e20dff1a4313d2b482ab88420dc47e481336a65a655b9b` / `e1593fe53d0025dd96de3156e7bd94c9a03038bd4bd98324b9ef356cf095ade8` |
| Final `sdkconfig` | `56d0f7239a7e3c5ccb688293c5f1aea3dff957daade95baf8c2569bdc8b0d712` |
| Seed / final `dependencies.lock` | `c811bfe049e3c99fa129fa3050d4807955db7d0aa90e6e97a42e84034f158a39` / `18efb5df229b4e429b9a2119a4b069346ef4a7d5ea180fdfcee97ac1c87661a2` |
| Live-layout CSV / generated partition-table binary | `14c937dd21618f5878ad48633d52c3b8473170bb28b30b353d2d64cdb4640d49` / `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f` |
| Final build log (`E:/v6/m1-afe-diag-s11-build.log`) | `bab3f27eab097ee9631a38e7ff7119140512401aa5bee6b5fa4c6377e7ca4c89` |
| Application / ELF | `5038ed6e5649e8d11794dcdfbffae30d0a2e89cecceb182e00d424924da00a74` / `6a050109fdb821e7464ae9418bb6d137053ddbc550fa9b632cf093e69b57660a` |

The 3,072-byte generated partition table matches the previously recorded live-layout hash `ef0039b6…`. This is a build-input comparison, not a new device read or a partition write. The final `sdkconfig` has `CONFIG_CLAW4_M1_INPUT_DIAGNOSTICS=y`, M0 diagnostics unset, device AEC and WakeNet enabled, `wn9_nihaoxiaozhi_tts` selected, the fixed NAS OTA URL, and `partitions/claw4-live-m1.csv`. The final project description selects staged `components/78__esp-wifi-connect`; its 23-file reviewed inventory and both NVS fail-closed paths passed the existing guards. The compiled command database includes AFE, Claw4 audio, and local Wi-Fi sources.

## Reproduction and limits

Staging ran `py -3.14 -B tools/v6/stage_device.py --upstream E:/workbuddy/claw4-v6/vendor/xiaozhi-esp32 --destination E:/v6/m1-afe-diag-s11 --variant m1 --m1-input-diagnostics`. The build ran the pinned Python 3.12 interpreter at `E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe` with `tools/v6/build_device.py --source E:/v6/m1-afe-diag-s11 --variant m1 --idf E:/workbuddy/claw4-v6/vendor/esp-idf --tools E:/workbuddy/claw4-v6/toolchains/idf61`. The V6 Host suite ran **140 tests, 0 failures/errors**; manifest JSON parsing and identity assertions passed; `git diff --check` passed.

No COM7, device, Flash, NVS, NAS, or shared `E:/v6/s1` operation occurred. S-08 and earlier DEVICE results are historical and are **not** inherited by this Candidate. AFE runtime counters, physical microphone-to-Wake recognition, WebSocket Voice, AEC effectiveness, and M1 Preflight remain `HARDWARE_VERIFY_REQUIRED / NOT_VERIFIED`. An independent source/build review is required before any hardware handoff; this build does not establish M1 PASS or change the M0 `CHANGES_REQUIRED` gate.
