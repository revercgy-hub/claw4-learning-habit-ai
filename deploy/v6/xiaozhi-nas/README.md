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

The WebSocket endpoint is `ws://192.168.3.100:7444/xiaozhi/v1/`. Port 7443 remains plain HTTP; this port change does not add TLS. A running container, OTA response, and WebSocket handshake prove service reachability only. M1 acceptance still requires the same Candidate and service configuration to pass the 20-round voice and fault matrix in `docs/v6/V6_M1_XIAOZHI_NAS_VOICE.md`.
