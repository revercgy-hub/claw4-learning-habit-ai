# Claw4 Learning V6 M0 port

This is an incremental hardware candidate, not a completed M0 or Voice acceptance.

- Upstream XiaoZhi v2.5.0 (`ac6deed3`) remains the runtime owner.
- LCD driver `esp_lcd_nv3051f.c/.h` copied from CloudZao/MetalioClaw4
  `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`, unchanged.
- Electrical I2S format and GPIO map derive from the same Metalio board.
  The audio adapter uses bounded buffers/timeouts and upstream AudioCodec APIs.
- C5 SDIO configuration and local audio-module commands were cross-checked against
  AlayaElla/MetalioClaw4-AgentUI `08cbaca9b5fd0128fd996b017a5e47a10d082e81`.
- MIT license retained in LICENSE. No AgentUI UI, protocol, power-management
  hooks, Bluetooth application, LearningScreen or VoiceSessionPort is copied.

`CONFIG_CLAW4_M0_DIAGNOSTICS=y` replaces only the app entry point with a local
hardware harness. It initializes the upstream AudioService, displays a touch
button, records three seconds only after a tap, then plays it locally. It starts
Wi-Fi but never the application protocol, OTA check, ASR, LLM or upload.
Disabling the option restores upstream main.cc for the later M1 stage.

Implemented candidate: NV3051F/GT911, TCA9555, audio module local mode, stereo32
I2S slave input/reference, amplifier gating, C5 SDIO configuration. Actual panel
and microphone/reference channel order must be checked on hardware.
Camera/SD and power-key integration remain outstanding; no M0 PASS claim.

Build with tools/v6/stage_device.py into a fresh short ASCII path, then
tools/v6/build_device.py. Stage script must match the pinned upstream anchors.
The generated partition CSV preserves every existing range and renames only
resources to assets. tools/v6/flash_plan.py refuses any NVS writes or partition
movement. Full device backup is mandatory before using the plan.
