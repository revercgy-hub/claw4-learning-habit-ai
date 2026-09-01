#!/usr/bin/env bash
# ============================================================
# Claw4 Bring-up 基线快照
#
# 用途: 落实 CLAW4_BRINGUP_PROMPT 第二节「第一原则: 先保留官方可运行基线」
#       在任何修改之前, 完整记录工具链 / 仓库 / 配置 / 固件的可复现指纹
#
# 用法: bash tools/bringup/capture_baseline.sh [仓库路径] [固件路径]
# 输出: docs/BASELINE.md  (同时打印到 stdout)
#
# 退出码: 0 = 成功   1 = 仓库路径无效
# ============================================================

REPO="${1:-D:/claw4/MetalioClaw4}"
FW="${2:-$REPO/Metalio_Claw4_Latest.bin}"
OUT_DIR="$(cd "$(dirname "$0")/../.." && pwd)/docs"
OUT="$OUT_DIR/BASELINE.md"
TS="$(date '+%Y-%m-%d %H:%M:%S %z')"

[ -d "$REPO" ] || { echo "错误: 仓库路径不存在: $REPO" >&2; exit 1; }
mkdir -p "$OUT_DIR"

sha() { [ -f "$1" ] && (command -v sha256sum >/dev/null 2>&1 && sha256sum "$1" | cut -c1-64 || echo "n/a") || echo "n/a"; }

{
echo "# Claw4 Bring-up 基线快照"
echo
echo "> 本文件在**任何修改之前**生成, 是官方可运行基线的可复现指纹。"
echo "> 生成脚本: \`tools/bringup/capture_baseline.sh\`"
echo
echo "| 项 | 值 |"
echo "|:---|:---|"
echo "| 生成时间 | $TS |"
echo "| 仓库路径 | \`$REPO\` |"
echo "| IDF_PATH | \`${IDF_PATH:-未设置}\` |"
echo
echo "---"
echo

# ---------- 1. 主机环境 ----------
echo "## 1. 主机环境"
echo
echo "| 项 | 值 |"
echo "|:---|:---|"
echo "| OS | $(uname -s -r 2>/dev/null) |"
echo "| 主机名 | $(hostname 2>/dev/null) |"
echo "| Git | $(git --version 2>/dev/null || echo 'n/a') |"
echo "| IDF 版本 | $(idf.py --version 2>/dev/null | head -1 | sed 's/|/\\|/g' || echo 'n/a') |"
echo "| CMake | $(cmake --version 2>/dev/null | head -1 || echo 'n/a') |"
echo "| Ninja | $(ninja --version 2>/dev/null || echo 'n/a') |"
echo "| Python(PATH) | $( { python --version || python3 --version; } 2>&1 | head -1 || echo 'n/a') |"
echo "| Python(IDF venv) | \`${IDF_PYTHON_ENV_PATH:-未激活}\` |"
echo

# ---------- 2. 工具链 ----------
echo "## 2. 工具链版本"
echo
echo '```'
if [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/tools/idf_py_actions/.." ]; then :; fi
for t in riscv32-esp-elf-gcc xtensa-esp-elf-gcc openocd; do
    if command -v "$t" >/dev/null 2>&1; then printf '%-24s %s\n' "$t" "$($t --version 2>&1 | head -1)"; fi
done
[ -d "$IDF_PATH/tools" ] && printf '%-24s %s\n' "IDF tools dir" "$IDF_PATH/tools"
echo '```'
echo

# ---------- 3. 仓库状态 ----------
echo "## 3. 仓库状态"
echo
cd "$REPO" || exit 1
echo "| 项 | 值 |"
echo "|:---|:---|"
echo "| Commit | \`$(git rev-parse HEAD 2>/dev/null)\` |"
echo "| 短 SHA | \`$(git rev-parse --short HEAD 2>/dev/null)\` |"
echo "| 分支 | \`$(git rev-parse --abbrev-ref HEAD 2>/dev/null)\` |"
echo "| 提交日期 | $(git log -1 --format=%ci 2>/dev/null) |"
echo "| Tag | \`$(git describe --tags 2>/dev/null || echo '无')\` |"
echo "| 工作区 | $( [ -z "$(git status --porcelain 2>/dev/null)" ] && echo '干净' || echo '**有未提交改动**' ) |"
echo
SUBS="$(git submodule status 2>/dev/null)"
echo "### Submodule"
echo
if [ -z "$SUBS" ]; then
    echo "_无 submodule_"
else
    echo '```'
    echo "$SUBS"
    echo '```'
fi
echo
echo "### 最近 5 条提交"
echo
echo '```'
git log -5 --format='%h  %ci  %s' 2>/dev/null
echo '```'
echo

# ---------- 4. sdkconfig 指纹 ----------
echo "## 4. sdkconfig 指纹"
echo
SDK="$REPO/sdkconfig"
if [ -f "$SDK" ]; then
    echo "| 项 | 值 |"
    echo "|:---|:---|"
    echo "| SHA256 | \`$(sha "$SDK")\` |"
    echo "| 大小 | $(wc -c < "$SDK") bytes |"
    echo "| 行数 | $(wc -l < "$SDK") |"
    echo
    echo "### 关键配置项"
    echo
    echo '```'
    grep -E '^CONFIG_IDF_TARGET=|^CONFIG_IDF_TARGET_ESP|^CONFIG_PARTITION_TABLE_OFFSET=|^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=|^CONFIG_ESPTOOLPY_FLASH(SIZE|MODE|FREQ)=|^CONFIG_SPIRAM=|^CONFIG_SPIRAM_MODE=|^CONFIG_SPIRAM_SPEED=|^CONFIG_ESP32P4_SELECTS_REV|^CONFIG_ESP_CONSOLE|^CONFIG_ESP_HOSTED_SDIO_SLOT=|^CONFIG_ESP_HOSTED_SDIO_BUS_WIDTH=|^CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=|^CONFIG_SLAVE_IDF_TARGET' "$SDK" 2>/dev/null | sed 's/^/  /'
    echo '```'
    echo
    echo "> ⚠️ 若后续出现 Flash 相关异常, 先核对 \`CONFIG_ESPTOOLPY_FLASHMODE\` 的布尔项与字符串项是否一致。"
else
    echo "_sdkconfig 不存在_"
fi
echo

# ---------- 5. 分区表 ----------
echo "## 5. 分区表"
echo
PT="$(grep -oE '^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="[^"]+"' "$SDK" 2>/dev/null | cut -d'"' -f2)"
PTF="$REPO/${PT:-partitions/v1/32m_dual.csv}"
echo "文件: \`$PT\`"
echo
if [ -f "$PTF" ]; then
    echo "| SHA256 | \`$(sha "$PTF")\` |"
    echo "|:---|:---|"
    echo
    echo '```'
    cat "$PTF"
    echo '```'
    echo
    echo "> 注意: 该表**无 factory 分区**, 采用 \`ota_0\` / \`ota_1\` 双系统 (对应 OpenClaw / ESPClaw)。"
else
    echo "_未找到分区表文件: $PTF_"
fi
echo

# ---------- 6. 出厂固件 ----------
echo "## 6. 出厂固件基线"
echo
if [ -f "$FW" ]; then
    MAGIC=$(od -An -tx1 -j 8192 -N 1 "$FW" 2>/dev/null | tr -d ' ')
    echo "| 项 | 值 |"
    echo "|:---|:---|"
    echo "| 文件 | \`$(basename "$FW")\` |"
    echo "| 大小 | $(wc -c < "$FW") bytes |"
    echo "| SHA256 | \`$(sha "$FW")\` |"
    echo "| 0x2000 magic | \`0x${MAGIC:-??}\` $( [ "$MAGIC" = "e9" ] && echo '(完整 flash 镜像 ✓)' || echo '(⚠️ 与预期不符)' ) |"
    echo
    echo "**回滚命令** (设备变砖时的唯一保险):"
    echo
    echo '```bash'
    echo "esptool.py --chip esp32p4 -p <COMx> -b 921600 write_flash 0x0 $(basename "$FW")"
    echo '```'
else
    echo "_未找到出厂固件: \`$FW\`_"
    echo
    echo "> ⚠️ 无回滚基线前, 严禁自行烧录或改动 C5 相关配置。"
fi
echo

# ---------- 7. ESPClaw 附属镜像 ----------
echo "## 7. ESPClaw 附属镜像 (ota_1)"
echo
CLAWDIR="$REPO/esp_claw_bin"
if [ -d "$CLAWDIR" ]; then
    echo "| 文件 | 大小 | SHA256 |"
    echo "|:---|---:|:---|"
    for b in "$CLAWDIR"/*.bin; do
        [ -f "$b" ] || continue
        echo "| \`$(basename "$b")\` | $(wc -c < "$b") | \`$(sha "$b" | cut -c1-16)…\` |"
    done
    echo
    echo "> 烧录地址以文件名中的 \`_0x...\` 段为准; 分区表偏移必须对齐 \`0x9000\`。"
else
    echo "_无 esp_claw_bin 目录_"
fi
echo

# ---------- 8. 待补实测项 ----------
echo "## 8. 待实测填充 (接机后填写)"
echo
echo "以下字段只能来自实机, 接机后逐项填入并标注来源:"
echo
echo '```'
cat <<'EOF'
P4 Revision          : ______   来源: ______
P4 CPU Freq          : ______   来源: ______
C5 Revision          : ______   来源: ______
C5 Firmware Version  : ______   来源: ______
Board Revision       : ______   来源: ______
SKU                  : ______   来源: ______
Internal SRAM        : ______   来源: heap_caps API
PSRAM total / free   : ______   来源: heap_caps API
Flash size / mode    : ______   来源: esptool / boot log
Display controller   : ______   来源: ______   (NV3051F? FL7707N?)
Touch controller     : ______   来源: ______
Camera sensor        : ______   来源: ______
P4 调试口 COM        : COM__   描述符: USB JTAG/serial debug unit
EOF
echo '```'
echo
echo "---"
echo
echo "_本快照由 \`tools/bringup/capture_baseline.sh\` 自动生成, 请勿手工编辑。_"

} | tee "$OUT"

echo
echo "已写入: $OUT"
