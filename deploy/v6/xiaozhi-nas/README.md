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
