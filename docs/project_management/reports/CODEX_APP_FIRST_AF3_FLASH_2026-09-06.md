# CODEX-APP-FIRST-001 AF3-5 ota_0 应用刷写记录

> 日期：2026-09-06  
> 负责人：Codex  目标：COM7 / ESP32-P4  
> 结果：`FLASH_PASS_HASH_VERIFIED`

## 1. 授权边界

用户明确授权本批后续刷机。本次严格执行 `ota_0 application-only`：只写应用地址 `0x200000`，未执行 `erase_flash`，未写 bootloader、partition table、`ota_1`、C5、eFuse 或其他地址。

## 2. 候选与目标核对

- 镜像：`E:\workbuddy\claw4-idf-cold-c5-20260906\xiaozhi.bin`
- 大小：`9,182,112 B`
- SHA-256：`EF2AEA459A13969DA10D7F9F1A6F94740CDF06EBDA835F3C9E151FC32A11F9BB`
- 串口：`COM7`
- 芯片：ESP32-P4 revision v1.3
- USB 模式：USB-Serial/JTAG
- MAC：`80:f1:b2:d2:ed:14`

## 3. 执行命令与证据

执行的是 `esptool.py v4.12.0` 的单地址写入：

```text
python -m esptool --chip esp32p4 -p COM7 -b 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 32MB --flash_freq 40m \
  0x200000 E:\\workbuddy\\claw4-idf-cold-c5-20260906\\xiaozhi.bin
```

关键结果：

```text
Wrote 9182112 bytes (4826934 compressed) at 0x00200000
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

写后只读连通性检查再次识别同一 ESP32-P4/MAC，并能通过 COM7 复位连接。

## 4. 当前结论与下一步

`ota_0` 应用镜像刷写和写后校验通过。该镜像尚未配置真实 endpoint/signer，因此不能据此声明认证、今日任务拉取、Wi-Fi/TLS、事件 ACK 或 PWA 恰好一次已在真机通过。下一步由用户观察屏幕是否正常进入 Home/Learning，并在提供安全凭据注入方案后再做真实联网验收；不得把 demo 的“backend 未配置”诊断误认为网络失败。

