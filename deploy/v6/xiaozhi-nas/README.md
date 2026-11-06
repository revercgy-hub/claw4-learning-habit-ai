# Claw4 V6 XiaoZhi NAS service

The deployment is isolated at `/vol2/1008/docker/xiaozhi-v6` on the Minisforum N5 (`192.168.3.100`, linux/amd64). The Compose file pins XiaoZhi server v0.9.6 by digest and binds WebSocket port 7444 and HTTP/OTA port 7443 to the NAS LAN address (container ports remain 8000/8003). It uses the NAS's existing Ollama `qwen3.5:2b`, local SenseVoiceSmall ASR, and upstream EdgeTTS. EdgeTTS sends synthetic TTS text to an external provider; do not use personal or child content during this validation stage.

The tagged [upstream Compose](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/main/xiaozhi-server/docker-compose.yml) and [configuration](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/v0.9.6/main/xiaozhi-server/config.yaml) define the container paths and override schema. Copy `config.override.yaml` to `data/.config.yaml` on the NAS. Keep any future credentials only in the NAS-local override, never in Git.

SenseVoiceSmall `model.pt` is downloaded from [ModelScope](https://modelscope.cn/models/iic/SenseVoiceSmall) and must have SHA-256 `833ca2dcfdf8ec91bd4f31cfac36d6124e0c459074d5e909aec9cabe6204a3ea`, matching the [upstream model file](https://huggingface.co/FunAudioLLM/SenseVoiceSmall/commit/2b05887414d06ebf81c0256c82b295450dcfc765). Place it at `models/SenseVoiceSmall/model.pt` before starting Compose.

On the NAS, validate and operate the existing deployment with:

```sh
cd /vol2/1008/docker/xiaozhi-v6
sudo sha256sum models/SenseVoiceSmall/model.pt
sudo docker compose config --quiet
sudo docker compose up -d --no-build --pull never
sudo docker compose ps
curl -fsS http://192.168.3.100:7443/xiaozhi/ota/
```

## Edge TTS 7.3.1 repair

The pinned upstream image contains Edge TTS 7.2.6, which currently receives
HTTP 403 responses. The optional `compose.edge-tts-fix.yaml` builds a local
image from the same digest-pinned upstream base and installs only
`edge-tts==7.3.1`. The wheel is hash-pinned, binary-only, and installed with
dependency resolution disabled so the base image's existing dependencies
(`aiohttp`, `certifi`, `tabulate`, and `typingextensions`) are not upgraded.
The Docker build asserts the installed Edge TTS version. `.dockerignore`
limits its build context to the Dockerfile and hashed requirements, excluding
deployment data, model weights, and credentials. The original `compose.yaml`
remains the rollback configuration.

From the deployment directory, validate and build the candidate image, then
run a synthetic provider probe before changing the service:

```sh
cd /vol2/1008/docker/xiaozhi-v6
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml config --quiet
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml build xiaozhi
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml run --rm --no-deps --entrypoint sh xiaozhi -ec \
  'edge-tts --text "Synthetic Claw4 TTS connectivity probe." --voice zh-CN-XiaoxiaoNeural --write-media /tmp/edge-tts-probe.mp3 && test -s /tmp/edge-tts-probe.mp3'
```

The probe uses synthetic text only. Before rollout, record the candidate
image ID, merged Compose configuration, and NAS-local TTS provider/voice
configuration as the image/config/provider identity for this deployment.
Keep credentials out of Git and logs. This probe demonstrates that the
provider returned audio in the build environment; it does not prove device
playback or M1 acceptance.

After reviewing that identity and the probe result, apply the candidate image
to only the `xiaozhi` service:

```sh
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml up -d --no-build --pull never --force-recreate xiaozhi
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml ps xiaozhi
```

Rollback to the original digest-pinned upstream image and Compose configuration
with:

```sh
sudo docker compose -f compose.yaml up -d --no-build --pull never --force-recreate xiaozhi
```

## Ollama Qwen voice response repair

The optional `compose.voice-fix.yaml` builds a separate image from the same
digest-pinned XiaoZhi v0.9.6 base. `Dockerfile.voice` repeats the reviewed,
hash-locked Edge TTS 7.3.1 installation and applies a bounded patch to the
upstream Ollama provider. The patch script first requires the exact provider
source SHA-256 `0ce07df5f7f72b9e3a4ae4086eb62b4cc94f75ac5526ddda41a6600f335b6aa6`,
then checks each source anchor before changing it. The exact source fixture is
kept at `tools/v6/fixtures/ollama_provider_v096.py`; it was copied byte for byte
from the read-only v0.9.6 NAS source capture
`E:/v6/nas-latency-20261020-01/upstream-ollama-v096.py` and its hash is checked
by the Host tests.

For Qwen3-family models, the patched provider sends `reasoning_effort="none"`
through the OpenAI-compatible API and caps responses at 128 tokens with
temperature 0.3. Qwen3.5 no longer receives the ineffective `/no_think` text
prefix; older Qwen3 keeps that prefix on a deep copy of the dialogue. Other
models do not receive `reasoning_effort`. The existing stream, content/tool
call handling, and model/LLM/TTS/ASR selection stay in place. The OpenAI client
uses a 20-second request timeout and zero automatic retries. The timeout
bounds HTTP request waiting; it is not a hard end-to-end wall-clock deadline
for consuming a streamed response.

Build and exercise the candidate with synthetic content before rollout:

```sh
cd /vol2/1008/docker/xiaozhi-v6
sudo docker compose -f compose.yaml -f compose.voice-fix.yaml config --quiet
sudo docker compose -f compose.yaml -f compose.voice-fix.yaml build xiaozhi
```

Record the image ID and merged Compose configuration, then run the existing
synthetic Voice probe and validate the provider behavior before applying the
overlay to only `xiaozhi`. The root task owner performs NAS build, probe, and
rollout; this repository change does not claim those checks have run.

There are two rollback levels. To remove only the Ollama patch while retaining
the Edge TTS 7.3.1 repair, use the previous TTS-fix overlay and its image:

```sh
sudo docker compose -f compose.yaml -f compose.edge-tts-fix.yaml up -d --no-build --pull never --force-recreate xiaozhi
```

To return to the upstream server image as well, use the original Compose file:

```sh
sudo docker compose -f compose.yaml up -d --no-build --pull never --force-recreate xiaozhi
```

The WebSocket endpoint is `ws://192.168.3.100:7444/xiaozhi/v1/`. Port 7443 remains plain HTTP; this port change does not add TLS. A running container, OTA response, and WebSocket handshake prove service reachability only. M1 acceptance still requires the same Candidate and service configuration to pass the 20-round voice and fault matrix in `docs/v6/V6_M1_XIAOZHI_NAS_VOICE.md`.

## M1 ASR content and spoken-answer prompt repair

`compose.asr-voice-fix.yaml` builds an additional isolated image from the same
digest-pinned XiaoZhi v0.9.6 base. `Dockerfile.asr-voice` repeats the hashed
Edge TTS 7.3.1 install and the exact-source-guarded Ollama patch, then applies
the separately owned `patch_asr_voice.py` to the upstream ASR base provider and
copies `claw4-m1-voice-prompt.txt` into the image. The S-20 ASR patch is already
root-integrated in the build context. This package does not claim the integrated
image has been built or probed.

The deployment-specific template includes `{{ base_prompt }}` so the existing
Chinese and privacy instructions remain in the rendered prompt. It adds short
spoken-answer guidance: answer first, usually in one or two sentences, without
numbering, Markdown, emoji, or introductory filler. It contains no fixed
answers, command routing, or emotion analysis. The override adds only the
`prompt_template: claw4-m1-voice-prompt.txt` selector; model, provider, ASR,
TTS, auth, endpoint, ports, and the existing prompt text remain as configured.
The generic upstream `agent-base-prompt.txt` is unchanged.

This Compose overlay also maps `host.docker.internal` to the NAS LAN address
`192.168.3.100` for the `xiaozhi` container only. The previous Docker
`host-gateway` route resolved to `172.17.0.1`, where Ollama port 11434 refused
connections; the NAS LAN address is the verified route. Compose normally
merges `extra_hosts` lists, which can leave both the stale `host-gateway` and
the new LAN entry active. The `!override` tag replaces the list completely;
it requires Docker Compose 2.24.4 or newer (the NAS has 2.40.3). Verify the
merged config resolves `xiaozhi.extra_hosts` to exactly the single list entry
`host.docker.internal=192.168.3.100`, with no `host-gateway` residue. This is a
container hostname-routing change only. It does not change Ollama, the provider
URL or model, ASR/TTS, authentication, prompt, ports, or any other service.
The existing `claw4-xiaozhi-v6:voice-asr-20261020-01` image remains reusable;
this mapping does not require a rebuild.

Before changing the NAS-local `data/.config.yaml`, retain a private owner-only
backup and enforce the compare-and-swap guard against the expected original
configuration hash. Do not print or copy the configuration into logs or Git:

```sh
cd /vol2/1008/docker/xiaozhi-v6
cfg=data/.config.yaml
expected=3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438
actual=$(sudo sha256sum "$cfg" | awk '{print $1}')
test "$actual" = "$expected" || { echo "config hash changed; stop" >&2; exit 1; }
backup=/root/xiaozhi-config.pre-asr-voice-20261020-01.yaml
sudo cp -a "$cfg" "$backup"
sudo chmod 600 "$backup"
sudo sha256sum "$backup"
```

Only after the guard and private backup succeed, add the single top-level
`prompt_template: claw4-m1-voice-prompt.txt` key to the NAS-local config. Keep
all existing keys and values. Record only the resulting config hash, not its
contents. Validate and build the overlay with synthetic content:

```sh
sudo docker compose -f compose.yaml -f compose.asr-voice-fix.yaml config --quiet
sudo docker compose -f compose.yaml -f compose.asr-voice-fix.yaml build xiaozhi
```

Before any rollout, record image ID, config hash, template SHA-256, and the
integrated ASR patch source identity. Confirm the running service uses
`/opt/xiaozhi-esp32-server/claw4-m1-voice-prompt.txt` as its selected template
in the newly created container, that no stale prompt cache overrides the new
selection, and that the rendered prompt reflects the template (record renderer
hash and length). Confirm the NAS environment does not inherit device-specific
`DEVICE` state. Use synthetic speech only. These checks and rollout belong to
the root NAS owner. The Compose overlay changes only the `xiaozhi` image/build
and its exact hostname mapping; it does not add environment variables or ports.

For rollback, validate the retained L-20 image and private backup, and compare
the current config against the expected pre-rollback SHA-256 before changing
anything. Any mismatch stops the procedure. After restoring the backup, enforce
its full SHA-256 and confirm the resolved Compose image maps to the exact
retained L-20 image ID before recreating the service. Keep the repaired host
route during rollback: the old `host-gateway` mapping is unreachable from this
NAS. The root NAS owner keeps a private one-key Compose overlay at
`evidence/asr-prompt-20261106-01/compose.rollback-route.yaml`; it must contain
only this `xiaozhi` override (the `!override` tag is required so the old list
is replaced):

```yaml
services:
  xiaozhi:
    extra_hosts: !override
      - "host.docker.internal:192.168.3.100"
```

Docker Compose 2.24.4 or newer is required for `!override`; the NAS runs
2.40.3. Validate the merged configuration and normalize the CLI's list-form
`extra_hosts` before asserting it equals exactly
`["host.docker.internal=192.168.3.100"]`, with no `host-gateway` residue. Do
not use `compose.yaml` or `compose.voice-fix.yaml` alone for rollback.

```sh
set -eu
backup=/root/xiaozhi-config.pre-asr-voice-20261020-01.yaml
expected_image_id=sha256:34fb7e5daa3cf24818b459e95b0eb772063788fc52b331841e5780beed48f0cd
expected_backup_sha=3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438
expected_current_config_sha=e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911

backup_sha=$(sudo sha256sum "$backup" | awk '{print $1}')
[ "$backup_sha" = "$expected_backup_sha" ] || { echo "backup SHA mismatch; stop" >&2; exit 1; }
image_id=$(sudo docker image inspect --format '{{.Id}}' claw4-xiaozhi-v6:voice-20261020-01)
[ "$image_id" = "$expected_image_id" ] || { echo "retained L-20 image identity mismatch; stop" >&2; exit 1; }
current_config_sha=$(sudo sha256sum data/.config.yaml | awk '{print $1}')
[ "$current_config_sha" = "$expected_current_config_sha" ] || { echo "current config CAS mismatch; stop" >&2; exit 1; }

sudo cp -a "$backup" data/.config.yaml
restored_config_sha=$(sudo sha256sum data/.config.yaml | awk '{print $1}')
[ "$restored_config_sha" = "$expected_backup_sha" ] || { echo "restored config SHA mismatch; stop" >&2; exit 1; }
rollback_route_overlay=evidence/asr-prompt-20261106-01/compose.rollback-route.yaml
sudo docker compose -f compose.yaml -f compose.voice-fix.yaml -f "$rollback_route_overlay" config --quiet
resolved_image=$(sudo docker compose -f compose.yaml -f compose.voice-fix.yaml -f "$rollback_route_overlay" config --format json | python3 -c 'import json,sys; print(json.load(sys.stdin)["services"]["xiaozhi"]["image"])')
[ -n "$resolved_image" ] || { echo "resolved Compose image missing; stop" >&2; exit 1; }
resolved_image_id=$(sudo docker image inspect --format '{{.Id}}' "$resolved_image")
[ "$resolved_image_id" = "$expected_image_id" ] || { echo "resolved Compose image identity mismatch; stop" >&2; exit 1; }
sudo docker compose -f compose.yaml -f compose.voice-fix.yaml -f "$rollback_route_overlay" up -d --no-build --pull never --force-recreate xiaozhi
```

Keep the original `compose.yaml` and the L-20 image available. Do not fall back
to the upstream Edge TTS 7.2.6 image during this rollback path.

## Connection-time Ollama preload candidate

`compose.prewarm.yaml` is an additional, unrolled candidate overlay. Resolve it
in this order: `compose.yaml`, `compose.asr-voice-fix.yaml`, then
`compose.prewarm.yaml`. The accepted ASR overlay supplies the LAN host alias;
the last overlay changes only the image tag and build recipe. The candidate
starts from the same digest-pinned upstream image and retains the hashed Edge
TTS repair, Ollama and ASR patches, and prompt. It adds a hash-guarded private
connection hook and helper. The accepted `9cd9…` image and `e568…` config stay
unchanged until independent review and root-owned isolated validation.

After an authenticated connection passes the `need_bind` guard, the helper may
schedule one daemon worker for the reviewed local Ollama host and `qwen3.5:2b`.
It checks residency, verifies the installed model digest, and makes one native
empty-prompt generation request only when needed. It sends no speech, chat or
history; no keep-alive or generation options; and does not pull models. Failures
do not stop normal initialization. HTTP response bodies are capped at 64 KiB
and each residency/installed-model probe has a 3-second socket timeout. The
empty generation preload has a 15-second socket timeout, leaving margin over
the observed 5.577-second cold request. These are per-operation/read timeouts,
**not** a strict whole-worker wall-clock deadline: a peer that continuously
drips data can occupy the sole
worker longer, but further connections cannot accumulate workers. Only status,
duration, load duration and error class are logged.

Root should validate the three-file Compose merge, retained LAN alias, exact
image ID and config SHA, then run isolated synthetic probes before considering
rollout. A rollback must use the accepted ASR overlay so the LAN alias remains;
base Compose alone restores the old gateway mapping. Never use this candidate
as evidence for the earlier runtime or device sessions.
