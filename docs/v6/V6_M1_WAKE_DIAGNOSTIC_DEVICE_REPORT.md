# S-08 M1 Wake diagnostic Candidate: app-only device check

**Status:** `DEVICE_DIAGNOSTIC_COMPLETE / WAKE_AND_WS_NOT_VERIFIED` (2026-09-27)  
**Candidate:** `claw4-learning-v6-m1-wake-diag-s08-20260926-01`  
**Independent review:** R-05 `PASS` on manifest revision `36d6443`; merged source `70d463938526ba768f889ebd548f8afd8a03f3af`  
**Firmware source:** `85b2265b65a09b67d9eabe9da5373bf18468161b`

This report records one app-only write/readback and passive post-boot diagnostics for S-08. It supersedes the device state in the [S-08 build Candidate report](V6_M1_WAKE_DIAGNOSTIC_CANDIDATE_REPORT.md), which was written before the device check. All results belong only to this Candidate and capture; no Wake, endpoint, or Voice result from another Candidate is inherited.

## Candidate identity and device layout

The reviewed Candidate app SHA-256 is `5619f2f2cd19cc42746fe1025a7c6390e037313573e1203093dfbf057889b27c`; ELF SHA-256 is `fc07bcb12a04b5738c1557bf1014fe39ad0659da8955c7ac5578826eb277bbb5`; manifest SHA-256 is `b2cccebf7162335b5dd8b511e5377253e1562d5a2139f6429ffb5e2d04155e60`. R-05 independently passed the manifest review before device work.

The single device owner verified an ESP32-P4 revision 1.3, 32 MiB flash device and matched its recorded inventory identity. Secure Boot and Flash Encryption were off. The live 4 KiB partition-table SHA-256 was `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`; the Candidate partition table matched the first 3,072 live bytes. Live `ota_0` is at `0x200000` with a 9 MiB size.

Before writing, the complete 9,437,184-byte `ota_0` image was saved privately (SHA-256 `40ec7d43f7b538bde1362af66c623e71513859bd808df6049bc5fc0fb14dea6f`). Its 3,135,568-byte prefix matched the previously recorded R-03 app hash prefix `1735fa6c…`; this is a before-write comparison only and transfers no R-03 device or Voice result to S-08.

## Write, readback, and boot

The only write was the approved app-only range at `0x200000`, exactly 3,136,144 bytes. Esptool reported `Hash of data verified`. A separate readback was byte-equal to the Candidate app and had SHA-256 `5619f2f2cd19cc42746fe1025a7c6390e037313573e1203093dfbf057889b27c`. The post-read partition table and `otadata` were byte-equal to their pre-write reads. No other flash range is reported as written.

A 60-second USB-reset capture loaded the expected ELF prefix `fc07bcb12`, ran from `ota_0`, and observed Wi-Fi connected to `505`. This was a USB reset and short boot observation, not a cold boot or long-term stability check. The capture observed an HTTP connection to NAS OTA endpoint `192.168.3.100:7443` and no connection to external `api.tenclass.net`. No connection to NAS WebSocket port 7444 was observed; the endpoint therefore remains `NOT_VERIFIED`.

## Passive input diagnostics

The redacted parser summary contains 48 accepted `M1_INPUT_DIAG` windows, 771,840 microphone samples, maximum interval RMS 536, maximum interval peak 6,080, and zero recorded I2S read failures. It contains zero exact M1 application Wake markers and zero recognized crash markers. Heap and PSRAM values are unknown. The private raw boot log SHA-256 is `797b888760edaac55db03d87081588438291490d3187f048df1741fc4b141b93`.

This is an ambient/passive input baseline, not a speech stimulus. The zero Wake-marker count does not establish Wake failure; WakeNet recognition was not directly tested. Aggregate mic level and read-failure counts do not prove AFE input quality, Wake, AEC effectiveness, or a Voice stage. The parser's zero crash-marker count is limited to its recognized markers in this capture.

## M1 and M0 status

No controlled speech was used. Voice Preflight remains `0/2–3` rounds, formal continuous 20 rounds have not started, and Wake recognition, WebSocket establishment, AEC effectiveness, TTS recapture/self-dialogue, and the remaining Voice flow are `NOT_VERIFIED`. This device check is not M1 PASS. M0 remains `CHANGES_REQUIRED`; AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`.

The device is currently powered on. COM7 was released after the capture. No cold boot is claimed. Post-capture esptool reads caused a later reset, so the passive capture session is no longer current. Any next user-assisted Wake/Voice test must use this same Candidate and image, establish and bind a fresh boot/session with contemporaneous logs as required by the runbook, and must not inherit the passive capture session or treat its baseline as a Preflight round.

## Evidence boundaries

Raw serial captures and detailed readback artifacts remain in approved private storage. The private parser summary `E:/v6/s08-device/s08-boot-summary.json` has SHA-256 `fd2b2141142bbbb2f647d3d619e97734c487eee79db37f1e89c166cab59d7cca`; the flash command output has SHA-256 `c6d5c9402a78df6d18bffe8def05b5bf96d0b650803c8e31d46f3749f63d9c69`. This report contains only the Candidate, partition, backup, readback, and evidence hashes needed for binding, plus redacted aggregate counters. No speech, transcript, credential, or raw log line is included.
