# A-04: bounded NAS Ollama host mapping repair

2026-11-06 recovery baseline: `ce19ee77f037c2973c5c835779d2926f81e1a34d`. The S-20/L-24 application image was built on 2026-10-20 from `150fe555a83105ad10852865ec363404f24d9a78`; the README-only R-13 correction does not change its build inputs. Do not rebuild that image merely to resume work.

## Observed dependency change

The current XiaoZhi container resolves `host.docker.internal` to `172.17.0.1`; TCP/API access at port 11434 is refused. The same container can read `/api/tags` at NAS LAN address `192.168.3.100:11434`, including the original `qwen3.5:2b` digest `324d162be6ca5629ae4517c8710434d0bd2d665bc94dbad46e9af8fbf8a2f0df`. The exact cause or time of the host-side binding change has not been established. See private safe evidence `E:/v6/nas-asr-prompt-20261106-01/ollama-current-probe.json` (NAS copy) and `ollama-route-compare.json` (local and NAS copies).

## Astra decision and scope

Fix the deployment-local hostname mapping for the XiaoZhi container only: `host.docker.internal` must resolve to the same NAS LAN address `192.168.3.100`. This is an internal reachability repair, with no V6 ADR or public contract change. Preserve the configured provider URL, model, ASR/TTS, authentication, prompt, ports, device OTA/WS endpoint and Voice Core. Do not modify the NAS Ollama service, gateway, listening sockets, firewall, Docker daemon, shared network or another container. No Flash, device or recovery operations are authorized by this task.

L-26 / Luna high may modify only `deploy/v6/xiaozhi-nas/compose.asr-voice-fix.yaml` (add one exact `extra_hosts` mapping) and the matching section of `deploy/v6/xiaozhi-nas/README.md` (explain the mapping and isolated rollback override). The original Compose files remain unchanged. Since the prior deployment's old `host-gateway` mapping is currently unreachable, an operational rollback to the retained L-20 image must preserve this same repaired host mapping using a private overlay limited to `extra_hosts`; it must not revert to the unreachable route or change any other service setting. Keep exact image/config checks from R-13. No worker may access NAS, SSH, COM7, hardware or build trees; root remains the sole execution owner.

Validate the Compose diff, exact mapping and rollback instructions, plus `git diff --check`; do not add implementation-mirroring tests for these static declarations. Return a single-task commit with static-check evidence. An independent Sol medium reviewer must review the integrated routing change before deployment. Root verifies actual merged Compose, exact retained/new image IDs, backup/config CAS, unchanged build inputs, post-recreation hostname resolution, model digest, template/source identity and fresh synthetic service/audio probes. Rollout and rollback are bounded to the XiaoZhi service. Device Voice/AEC/Preflight and formal 20-round status do not inherit service-side success.

Stop for a required public interface/ADR change, expanded irreversible operation, data-loss risk or unresolved scope conflict. M0 remains CHANGES_REQUIRED; skipped AP recovery is not redispatched.
