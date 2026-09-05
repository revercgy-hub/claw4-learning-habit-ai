# CODEX-APP-FIRST-001 AF3a 报告：IDF BUILD ONLY

- 任务：`CODEX-APP-FIRST-001 / AF3a`
- 分支：`codex/app-first-mvp-loop`
- 操作：仅使用现有 `E:\c` 构建镜像和 `E:\b` 构建目录；没有 app-flash、串口、Flash 读取/擦除、分区或 bootloader 修改。

## 命令

使用 `E:\workbuddy\claw4-idf-tools\python_env\idf5.5_py3.12_env`、ESP-IDF v5.5.4、CMake 3.30.2、Ninja 1.12.1：

```powershell
idf.py -C E:\c -B E:\b build
```

## 结果

- `idf.py build`：exit `0`，`Project build complete`。
- target：`esp32p4`；IDF：`5.5.4`。
- `E:\b\xiaozhi.bin`：`9,175,856 B`。
- SHA-256：`6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f`。
- ota_0 通过；构建工具报告既有 `ota_1` 溢出，按硬门禁保持不触碰 ota_1/分区。

## 限定结论

AF3a 只证明当前已批准设备镜像可重复构建；App-first 新增的 host sync/Backend 代码尚未接入官方设备镜像，因此 AF3 整体仍需候选冻结前完成镜像 manifest 对齐和静态检查。真机仍不需要连接。
