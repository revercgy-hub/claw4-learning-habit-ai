# A-07: bounded connection-time Ollama preload

2026-11-06. Base: `8892ae63cbfdf096088f3f7b9b59a304bc71c565`. Existing service stays at image `sha256:9cd9b4a1f64e46adf5b320bcf58bbb6e39ade16ca46ed713c824058544945476`, config `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`, until independent review and root-owned isolated validation succeed.

## Evidence and decision

Three serialized real-provider synthetic probes measured LLM first content at 6798.94/209.69/185.12 ms. A separate native request after **observed natural model expiry** took 5577 ms wall time, including 4962.08 ms reported loading. This establishes loading cost in that native request; it does not retrospectively attribute every millisecond of Preflight 04. Safe records are under `E:/v6/stage-profile-20261106-01/`. [Ollama documents empty-request preloading and ordinary model expiry](https://docs.ollama.com/faq#how-can-i-preload-a-model-into-ollama-to-get-faster-response-times).

Astra authorizes an internal deployment adapter: after upstream authentication and the existing `need_bind` early return, schedule a background preload of the **existing** selected `qwen3.5:2b` model. It may overlap the greeting/listening interval. It must not block component initialization, ASR or chat; create a new Voice owner; change public provider/base interfaces, WebSocket messages, endpoints, ports, prompts, model selection, generation options, V6 ADRs or Gate semantics. This decision changes no public contract. If implementation needs such a change, stop and return to Astra.

## Required bounds

- Recognize only the trusted Ollama provider module and the current model. Snapshot scalar values; do not retain a connection/provider object in the background worker. Limit the native target to the configured known local NAS alias/port, rejecting credentials, query/fragment, foreign hosts, unexpected paths and other providers/models.
- One process-wide in-flight worker for this backend/model; concurrent connections must not create duplicate preload work. Release state on every success/error path. No periodic timer, retry loop, indefinite residency or forced unload.
- Read `/api/ps`; if the selected model is resident, skip generation. Otherwise send a native empty `/api/generate` request for that model with streaming disabled. Do not specify `keep_alive`, change daemon/environment settings, or send speech, prompts or conversation history. Do not download/pull a missing model; verify the installed model identity before a preload request.
- Bound HTTP work and response sizes; describe socket/read versus wall-clock guarantees accurately. A stalled backend must not block Voice processing or accumulate workers. Use a daemon worker with no connection lifecycle reference. No new subprocess/service/privileged setting on the live NAS is permitted.
- Emit only bounded stage/status/duration/error-class metadata, never URLs, headers, credentials, transcripts or exception messages. Preload failure is nonfatal to the existing Voice path.

## S-22 ownership

Sol medium owns only these files: `deploy/v6/xiaozhi-nas/claw4_ollama_prewarm.py`, `patch_ollama_prewarm.py`, `Dockerfile.prewarm`, `compose.prewarm.yaml`, `.dockerignore`, `README.md`; `tools/v6/test_ollama_prewarm.py`, `tools/v6/fixtures/connection_v096.py`, `tools/v6/fixtures/.gitattributes`, and the exact Docker-input expectation in `tools/v6/test_ollama_voice_patch.py`. Existing Dockerfiles, Compose files, provider source/patches, config and firmware are forbidden changes. New local image tag: `claw4-xiaozhi-v6:voice-prewarm-20261106-01`. Compose order preserves the accepted ASR overlay and host mapping, then changes only image/build.

The new hash-guarded patch targets only the pinned upstream `core/connection.py` (expected SHA256 `b1365d1445a0f3c3fe4fe8ef6a22f3d76b8a2ae7098cd57c97869fcb9d05b53d`), adding the private helper hook after the binding guard. The helper is copied into the image; the Ollama provider stays unchanged. Reject source/anchor mismatches before writing. The fixture is the exact source bytes, not a reimplementation; use an exact-byte attribute.

Host tests must exercise the patched real initialization method and helper: bind/unknown-provider paths, nonblocking scheduling, single flight, resident skip, installed-model checks, target validation, bounded reads/errors, release after failure, privacy and continued initialization. Use deterministic fake transports/events; no NAS/network calls from tests. Run the complete existing V6 Python suite, report exact counts, and commit only allowed paths. Workers have no NAS/SSH/COM7/Flash/build access and must not revert others.

An independent Sol medium R-16 must review the immutable integration before root builds a new NAS image. Root alone owns build, image/source/config/manifest checks, isolated synthetic WS/voice-service probes, default-expiry behavior and any bounded rollout/rollback. Keep the current image/config for rollback. No device flash is needed. Prior service/device evidence remains bound to its old runtime; new DEVICE results require their own session. M0 remains CHANGES_REQUIRED; AP recovery remains SKIPPED_BY_USER / NOT_VERIFIED; formal 20 rounds remain unstarted.
