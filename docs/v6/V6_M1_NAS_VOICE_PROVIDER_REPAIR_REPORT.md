# L-21 NAS Voice provider 修复服务证据报告

记录日期：2026-10-20。此报告记录 L-20 修复经 R-12 复核后的 NAS build、rollout 和 provider 级合成探针结果。当前判定仅为 `NAS_VOICE_PROVIDER_SERVICE_VERIFIED / DEVICE_PREFLIGHT_PENDING`；不表示设备已播放、旧 S-17 失败已解决、Preflight 完成或 M1 验收通过。

## 来源、审查与修复边界

- L-20 source commit：`2e9be855f3382db7f8182adfd197e94f88583e95`；集成到本报告基线 `531ac23e8ed8608179ad83d42d163c0b8f40efa5`。R-12 独立审查为 `PASS`，范围仅包括 CODE/HOST/readiness，不是 NAS runtime 或设备验收。
- L-20 worker 的完整 Host 汇总为 `165/0/0/0`，定向测试为 `7/7`；Root 实际 NAS Docker build exit `0`。这些证据分别支持代码与构建状态，不等同设备验证。
- 修复针对 Qwen3 系列 provider 请求参数：统一 `max_tokens=128`、`temperature=0.3`；Qwen3 family 使用 `reasoning_effort="none"`。旧 Qwen3 保留 `/no_think` 行为，但对 Qwen3.5 不注入该文本，并在复制对话时使用深拷贝。OpenAI client 设置 20 秒 read timeout 与 `max_retries=0`；该值不是整个请求的总时限。未改 Voice/AEC 架构或 ADR，也未加入 Router。
- pre/post 环境中的 Ollama model 名称和 digest、NAS config SHA 保持不变。Rollout 只改变 image/build；原 L-17 TTS fixed image 保留为回退目标，未执行 rollback。没有刷写设备。

## NAS build 与 rollout

Root 的 Docker build 结果为 exit `0`，构建出的 image ID 为 `sha256:34fb7e5daa3cf24818b459e95b0eb772063788fc52b331841e5780beed48f0cd`，tag 为 `claw4-xiaozhi-v6:voice-20261020-01`。Predeployment identity 确认该 image 与 reviewed source 对应，Compose 等价性检查仅排除 image/build 字段，rollout 记录 `changed_image_only=true`、exit `0`、未回退。部署后同一 image 正在运行，restart count 为 `0`；config SHA-256 仍为 `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`，OTA HTTP 为 `200`，预期 WS endpoint 存在。

Prechange 环境记录 Ollama `0.30.7`、model `qwen3.5:2b`，digest `324d162be6ca5629ae4517c8710434d0bd2d665bc94dbad46e9af8fbf8a2f0df`；OpenAI SDK `2.8.1`、Edge TTS `7.3.1`。Prechange provider source SHA 为 `0ce07df5f7f72b9e3a4ae4086eb62b4cc94f75ac5526ddda41a6600f335b6aa6`；patched provider source SHA 为 `434751da49b9baf2aa6a1726c2f3202ad79b06a96e7ee27110268366580e22f9`。NAS config 和 model 身份未改变。

用于身份核对的 SHA-256 字段从 prechange/predeployment/build/provider JSON 原样抄录如下；每个十六进制摘要均为 64 位：

| 对象 | SHA-256 |
| --- | --- |
| Build log | `9c0c43dbd2f6ebe375df0a153cf5f9e5c0ce30252ec1643da5733e673f917303` |
| NAS model `qwen3.5:2b` | `324d162be6ca5629ae4517c8710434d0bd2d665bc94dbad46e9af8fbf8a2f0df` |
| Prechange provider source | `0ce07df5f7f72b9e3a4ae4086eb62b4cc94f75ac5526ddda41a6600f335b6aa6` |
| Patched provider source | `434751da49b9baf2aa6a1726c2f3202ad79b06a96e7ee27110268366580e22f9` |
| NAS config | `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438` |
| Merged Compose | `8fb898ea8afae81e095102be886ff6acbbd85d36ab91c73fdc6fd02428303a72` |
| `compose.yaml` | `8e420b89d0381608b5833078168c53c3c41b741fc9f0e971f2630c6d3d488f91` |
| `compose.voice-fix.yaml` | `9ee7e8e5bb9464d01150f14734a22375c29189d843fe4702be970eac10793bd0` |
| `Dockerfile.voice` | `785bcbf61e1977049973ae271639f61fec137fbe457d438de880508f0caa62f1` |
| `.dockerignore` | `fc8fb92582a4f6d16fee2464d60410b1e6273cc2d91d9dfc5bbdf1922a666c90` |
| `patch_ollama_voice.py` | `2ed7b14ec6810d56b96e4a026fe01498883630c044f1b1bec02539be63a87344` |
| `edge-tts.requirements.txt` | `259ba8659735e984f2a8d5e7d8c6708e926338e7bc66ee51ebaf42397f40e87e` |
| Isolated provider probe | `2de38552603b35a971b3d7a6aa18cb235b0c7f3bcb4c539ba6cbd6f339cb1d00` |
| Synthetic answer | `d4735e3a265e16eee03f59718b9b5d03019c07d8b6c51f90da3a666eec13ab35` |
| Generated TTS audio | `32c81da5dde3e7eff8c1878758d9e30d2f5617f07ab5ebbe524f205ba6ffb6d1` |

## Provider probes 与判定边界

部署后的 production-provider probe 首个内容耗时 `425 ms`、LLM 总耗时 `459 ms`，合成结果符合预期。TTS provider 生成 `6,480` bytes 音频，SHA-256 `32c81da5dde3e7eff8c1878758d9e30d2f5617f07ab5ebbe524f205ba6ffb6d1`；独立解码时长 `1,080 ms`、RMS `3,510`。隔离 patched-provider probe 的首内容/总耗时分别为 `456/488 ms`，预期结果匹配；同一自动生成音频 SHA 与解码摘要相符。两者是 provider/LLM/TTS 服务级合成证据，不是设备播放证据。

另两个请求参数探针均返回预期合成结果：native Ollama `think=false` 的首内容/总耗时为 `5,073/5,107 ms`，OpenAI-compatible `reasoning_effort="none"` 为 `307/342 ms`。这些是不同 API 调用的独立 synthetic probes，不是受控 before/after A/B，因此不单独证明延迟改进由某个参数造成，也不证明可解决此前约 55 秒慢答或答错现象。没有把它们描述成 AEC、near-end、Wake 或端到端 Voice 证据。

## 设备状态与阶段门禁

S-17 `preflight-02` 中用户反馈响应慢且答错的观察仍作为历史失败记录保留；本次只证明修复后的 NAS provider/service probe 能工作，不清除该设备缺口。修复后尚无新的真机验证，`Preflight=0/2–3`，正式 20 轮未开始，`M1` 未验收。Root 已询问用户是否准备进行新测试；在收到回复前不记录为已开始或通过。`M0=CHANGES_REQUIRED`；AP outage/recovery 仍为 `SKIPPED_BY_USER / NOT_VERIFIED`。

因此，当前范围状态为 `NAS_VOICE_PROVIDER_SERVICE_VERIFIED / DEVICE_PREFLIGHT_PENDING`。设备端实际播放、Wake recovery、AEC、near-end 与正式轮次仍需独立证据；本报告不授权、不执行设备操作。

## 私有证据索引

证据来自 `E:/v6/nas-latency-20261020-01/` 的指定 JSON 文件。下表记录各 JSON 的实际字节数和 SHA-256；不包含或读取敏感 raw 内容。

| JSON 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| `build-result.json` | 215 | `26d670ccb0c838c630df38101e5de4140ac8320394f316f8dcf7ae52d4901835` |
| `prechange-environment.json` | 710 | `365bc041f183d2f4053142464c291a0858c6073a03f8845f3b4798b29b1220b6` |
| `predeployment-identity.json` | 1,586 | `ad398012bd106b36c024977ee6f349502fd8d6e903e53af0b66b35d28906c9b9` |
| `rollout-result.json` | 144 | `73a41c9a3c632b6c5b69907a75ce8c94e8b4df672ea8ed310ad29b8d78d8b029` |
| `isolated-provider-patched.json` | 761 | `2de38552603b35a971b3d7a6aa18cb235b0c7f3bcb4c539ba6cbd6f339cb1d00` |
| `postdeployment-identity.json` | 1,226 | `c4c42d86f1a96d85c65fa0cbfb214fdab22276952f5b675f7a7e47537549cdeb` |
| `native-think-false.json` | 493 | `6baddf2cb1913d0bd7e7be9d5b1110e319b4764c0a348e9be9cf2c3378cb6eca` |
| `openai-reasoning-none.json` | 399 | `4e0fa967b861700cde8c56112fcd03dbc54df99017d845232740aa45e8acfa1b` |

Identity values copied from the JSON include the full 64-character SHA-256s for the config, model digest, provider source, image, build log, and generated audio. Raw logs, prompts, credentials, and user/device audio are not reproduced.
