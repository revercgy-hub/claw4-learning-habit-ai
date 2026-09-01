#!/usr/bin/env bash
# ============================================================
# Claw4 Bring-up 环境预检脚本 (Windows / Git Bash)
#
# 用途: 开跑 B001 之前, 确认工具链 / 串口 / 路径 / 固件全部就绪
# 用法: bash tools/bringup/preflight.sh [仓库路径] [出厂固件路径]
# 例:   bash tools/bringup/preflight.sh D:/claw4/MetalioClaw4 D:/claw4/firmware/baseline/Metalio_Claw4_Latest.bin
#
# 退出码: 0 = 全部通过(可开跑)  1 = 存在 P0 阻断项
# ============================================================

REPO="${1:-D:/claw4/MetalioClaw4}"
FW="${2:-$REPO/Metalio_Claw4_Latest.bin}"
REQUIRED_IDF="v5.5.4"

FAIL=0
WARN=0

pass()  { printf "  \033[32m[PASS]\033[0m %s\n" "$1"; }
fail()  { printf "  \033[31m[FAIL]\033[0m %s\n" "$1"; FAIL=$((FAIL+1)); }
warn()  { printf "  \033[33m[WARN]\033[0m %s\n" "$1"; WARN=$((WARN+1)); }
info()  { printf "        %s\n" "$1"; }
sect()  { printf "\n\033[1m%s\033[0m\n" "$1"; }

echo "=============================================="
echo " Claw4 Bring-up 环境预检"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo " 仓库: $REPO"
echo "=============================================="

# ---------- 1. 基础工具 ----------
sect "[1] 基础工具"

command -v git >/dev/null 2>&1 && pass "git: $(git --version)" || fail "git 未安装"

PYTHON=""
for p in python python3 py; do
    if command -v "$p" >/dev/null 2>&1; then PYTHON="$p"; break; fi
done
if [ -n "$PYTHON" ]; then
    PV="$($PYTHON --version 2>&1 | awk '{print $2}')"
    MAJOR=$(echo "$PV" | cut -d. -f1); MINOR=$(echo "$PV" | cut -d. -f2)
    if [ "$MAJOR" = "3" ] && [ "$MINOR" -ge 13 ] 2>/dev/null; then
        warn "系统 Python $PV 偏新, 建议改用 ESP-IDF 自带 Python (3.11/3.12)"
    else
        pass "Python: $PV"
    fi
else
    fail "未找到 python"
fi

# ---------- 2. ESP-IDF ----------
sect "[2] ESP-IDF 环境 (要求 $REQUIRED_IDF)"

if [ -z "$IDF_PATH" ]; then
    fail "IDF_PATH 未设置 — 请用 'ESP-IDF 5.5 PowerShell' 启动, 或先运行 export.sh"
    info "Windows: 开始菜单 → ESP-IDF 5.5 PowerShell"
else
    pass "IDF_PATH = $IDF_PATH"
    case "$IDF_PATH" in
        *" "*|*[!A-Za-z0-9:/\\._-]*) warn "IDF_PATH 含空格或非 ASCII 字符, 构建易失败";;
    esac

    if command -v idf.py >/dev/null 2>&1; then
        IDFV="$(idf.py --version 2>&1 | grep -oE 'v[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)"
        if [ "$IDFV" = "$REQUIRED_IDF" ]; then
            pass "idf.py 版本: $IDFV (匹配)"
        else
            fail "idf.py 版本为 $IDFV, 要求 $REQUIRED_IDF (C5 v1.0 需 >= v5.5.2)"
        fi
    else
        fail "idf.py 不在 PATH 中"
    fi

    [ -f "$IDF_PATH/export.sh" ] && pass "export.sh 存在" || warn "export.sh 缺失"
fi

command -v cmake >/dev/null 2>&1 && pass "cmake: $(cmake --version 2>&1 | head -1)" || fail "cmake 未安装"
command -v ninja >/dev/null 2>&1 && pass "ninja: $(ninja --version 2>&1)" || fail "ninja 未安装"
command -v esptool.py >/dev/null 2>&1 && pass "esptool.py 可用" || warn "esptool.py 不在 PATH (可由 IDF 环境提供)"

# ---------- 3. 仓库与路径 ----------
sect "[3] 仓库与路径规范"

if [ -d "$REPO" ]; then
    pass "仓库目录存在: $REPO"
    case "$REPO" in
        *[!A-Za-z0-9:/\\._-]*)
            fail "仓库路径含非 ASCII 字符(空格/中文), CMake/Ninja 构建风险极高"
            info "建议迁移到 D:/claw4/MetalioClaw4"
            ;;
        *" "*)
            fail "仓库路径含空格"
            ;;
        *) pass "路径为纯 ASCII, 无空格" ;;
    esac

    cd "$REPO" 2>/dev/null && {
        pass "git commit: $(git rev-parse --short HEAD 2>/dev/null)"
        info "完整: $(git rev-parse HEAD 2>/dev/null)"
        info "分支: $(git rev-parse --abbrev-ref HEAD 2>/dev/null)"
        info "日期: $(git log -1 --format=%ci 2>/dev/null)"

        DIRTY=$(git status --porcelain 2>/dev/null | head -5)
        [ -z "$DIRTY" ] && pass "工作区干净" || { warn "工作区有未提交改动:"; info "$DIRTY"; }

        git rev-parse --verify bringup-baseline >/dev/null 2>&1 \
            && pass "基线标签 bringup-baseline 已存在" \
            || warn "尚未打基线标签 (建议: git tag bringup-baseline)"

        SUBS=$(git submodule status 2>/dev/null)
        [ -z "$SUBS" ] && info "无 submodule" || info "submodule: $(echo "$SUBS" | wc -l) 个"
    }
else
    fail "仓库目录不存在: $REPO"
fi

# ---------- 4. 关键配置核验 ----------
sect "[4] sdkconfig 关键配置核验"

if [ -f "$REPO/sdkconfig" ]; then
    pass "sdkconfig 存在"

    grep -q '^CONFIG_IDF_TARGET="esp32p4"' "$REPO/sdkconfig" \
        && pass "Target = esp32p4" || fail "Target 不是 esp32p4"

    OFF=$(grep -oE '^CONFIG_PARTITION_TABLE_OFFSET=0x[0-9a-fA-F]+' "$REPO/sdkconfig" | cut -d= -f2)
    if [ "$OFF" = "0x9000" ]; then
        pass "分区表偏移 = $OFF (符合仓库要求, 注意非默认 0x8000)"
    else
        fail "分区表偏移 = ${OFF:-未设置}, 应为 0x9000"
    fi

    grep -q '^CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y' "$REPO/sdkconfig" \
        && pass "Flash = 32MB" || warn "Flash 配置非 32MB"

    grep -q '^CONFIG_SPIRAM=y' "$REPO/sdkconfig" \
        && pass "PSRAM 已启用" || warn "PSRAM 未启用"

    # 坑5: FLASHMODE 一致性
    QIO=$(grep -c '^CONFIG_ESPTOOLPY_FLASHMODE_QIO=y' "$REPO/sdkconfig")
    MODE=$(grep -oE '^CONFIG_ESPTOOLPY_FLASHMODE="[a-z]+"' "$REPO/sdkconfig" | cut -d'"' -f2)
    if [ "$QIO" = "1" ] && [ "$MODE" != "qio" ]; then
        warn "sdkconfig FLASHMODE 不一致: _QIO=y 但字符串为 '$MODE' (疑似手工编辑过)"
        info "已记录, 暂不修改; 若后续 Flash 异常, 此处为首个排查点"
    else
        pass "Flash mode 一致: $MODE"
    fi
else
    fail "sdkconfig 不存在"
fi

if [ -f "$REPO/partitions/v1/32m_dual.csv" ]; then
    pass "分区表 exists: partitions/v1/32m_dual.csv"
    info "无 factory 分区; 采用 ota_0 / ota_1 双系统"
else
    warn "未找到 partitions/v1/32m_dual.csv"
fi

# ---------- 5. 出厂固件 ----------
sect "[5] 出厂固件 / 回滚基线"

if [ -f "$FW" ]; then
    SIZE=$(wc -c < "$FW")
    pass "固件存在: $(basename "$FW") ($SIZE bytes)"
    command -v sha256sum >/dev/null 2>&1 && info "SHA256: $(sha256sum "$FW" | cut -c1-64)"

    # 校验是否为从 0x0 开始的完整镜像 (bootloader magic 0xE9 @ 0x2000)
    MAGIC=$(od -An -tx1 -j 8192 -N 1 "$FW" 2>/dev/null | tr -d ' ')
    if [ "$MAGIC" = "e9" ]; then
        pass "镜像结构校验通过 (0x2000 处 magic=0xe9 → 完整 flash 镜像, 烧录地址 0x0)"
        info "回滚命令: esptool.py --chip esp32p4 -p COMx -b 921600 write_flash 0x0 $(basename "$FW")"
    else
        warn "0x2000 处 magic=0x${MAGIC:-??}, 与完整 flash 镜像预期不符 — 请确认烧录地址"
    fi
else
    warn "未找到出厂固件: $FW"
    info "无回滚基线前, 请勿自行烧录 C5 相关操作"
fi

# ---------- 6. C5 从机固件 ----------
sect "[6] C5 从机固件 (Wi-Fi 命脉)"

C5HIT=$(find "$REPO" -iname '*c5*' -o -iname '*slave*' 2>/dev/null | grep -v '/.git/' | head -3)
if [ -z "$C5HIT" ]; then
    warn "仓库内未发现 C5 从机固件"
    info "README 未给出 C5 烧录步骤 — 出厂固件应已内置"
    info "策略: 第一轮 Bring-up 只跑出厂固件, 勿自行 build+flash"
else
    pass "发现 C5 相关文件:"; info "$C5HIT"
fi

# ---------- 7. 串口 ----------
sect "[7] 串口 / 设备连接"

PORTS=""
REG_RC=""
if command -v reg >/dev/null 2>&1; then
    PORTS=$(reg query 'HKLM\HARDWARE\DEVICEMAP\SERIALCOMM' 2>/dev/null | grep -oE 'COM[0-9]+' | sort -u)
    REG_RC=$?
fi

if [ -n "$PORTS" ]; then
    pass "检测到串口:"; info "$(echo "$PORTS" | tr '\n' ' ')"
    info "→ 在设备管理器确认哪个描述为 'USB JTAG/serial debug unit' (P4 主控口)"
elif [ "$REG_RC" != "0" ]; then
    # reg 存在但执行失败(多为安全策略拦截), 不能据此判定设备未连接
    warn "无法自动枚举串口 (reg 调用被拦截或返回错误) — 结论不可信, 请人工确认"
    info "人工确认: Win+X → 设备管理器 → 端口(COM 和 LPT)"
    info "         设备通电后应出现 4 个口, 其中 P4 主控口描述为 'USB JTAG/serial debug unit'"
else
    fail "未检测到任何串口设备"
    info "检查: ① USB 线是否支持数据 ② 设备是否通电 ③ 驱动是否安装"
fi
info "人工核对项: 设备通电后应出现 4 个串口 (P4调试 / 蓝牙CH340K / 4G log / 4G at)"

# ---------- 8. 磁盘 ----------
sect "[8] 磁盘空间"
df -h /c /d /e 2>/dev/null | awk 'NR==1 || /^\/dev\// || /^[A-Za-z]:/ {printf "        %s\n", $0}'
info "ESP-IDF v5.5.4 完整安装约需 10-15GB, 编译产物另计"

# ---------- 汇总 ----------
echo
echo "=============================================="
if [ "$FAIL" -eq 0 ]; then
    printf "\033[32m预检通过\033[0m (警告 %s 项) — 可开跑 B001\n" "$WARN"
    echo "=============================================="
    exit 0
else
    printf "\033[31m预检未通过: %s 项阻断, %s 项警告\033[0m\n" "$FAIL" "$WARN"
    echo "按 Bring-up Prompt 第二十五节要求: 修完阻断项再开跑, 不要猜测硬件规格"
    echo "=============================================="
    exit 1
fi
