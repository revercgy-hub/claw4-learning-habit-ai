# Claw4 目录地图（E 盘，本机专用）

> **机器专属文档**。记录 `E:\` 与 `E:\workbuddy\` 下所有 `claw4-*` 相关目录的身份、归属与「能不能动」。
> 生成于 2026-09-19，基于**只读实测**（`git worktree list` / `.git` 指针 / 目录内容比对）。
>
> ⚠️ **阅读前请先看 §0 三句结论** —— 这批目录的绝大多数**不能直接移动**，原因不是保守，是技术性的。

---

## §0 三句结论

1. **这批目录不是文件夹，大部分是 git worktree。**
   它们通过 **双向绝对路径**（worktree 里的 `.git` 文件 ↔ 父仓库 `.git/worktrees/<名>/gitdir`）与父仓库互焊。
   **用资源管理器 / `Move-Item` 搬动任何一个，都会立刻把它变成坏仓库**（`fatal: not a git repository`）。

2. **其中 8 个是在役资产**（活跃构建树、IDF 工具、学习后端、共享 venv、数据库备份、两个父仓库）
   ⇒ **一个都不能动**。动了不是"整理"，是**直接弄坏当前开发链**。

3. **真正可以自由搬迁的只有"纯目录"那一批**（无 git 元数据、已被取代的旧构建/侦察产物，§3）。

---

## §1 在役资产 —— 🚫 禁止移动

| 路径 | 是什么 | 为什么不能动 |
|---|---|---|
| `E:\a05c` | **活跃隔离构建树**：`s` = 源码镜像，`b` / `b2` = 构建产物 | ① **目录名长度是承重的**：本项目构建命令行有 379 段 `-I`、其中 212 段带树前缀 ⇒ 树路径每 +1 字符 ≈ 命令行 +212 字符，而 Windows 上限 32,767。② runner 有 **fail-closed 绑定守卫**：`-Src` 必须等于 manifest 里的 `isolated_src.origin`，换路径会直接 exit 3 |
| `E:\workbuddy\claw4-idf-tools` | **`IDF_TOOLS_PATH`**（含 `python_env/idf5.5_py3.12_env`） | python venv 的 `pyvenv.cfg` 与 Scripts 里**烧死了绝对路径**，搬完即失效 ⇒ 整个构建链报废 |
| `E:\workbuddy\claw4-domain-offline-stream` | 内含**共享 venv** `backend\.venv`（学习后端 + relay 都从这里启动） | 同上（venv 绝对路径烧死） |
| `E:\workbuddy\claw4-l1-ready` | **学习后端实体**（FastAPI + `.claw4_host_mvp.db`），T1 用的就是它 | 文档/脚本按绝对路径引用；且它是 3 个 worktree 之一（见 §2.3） |
| `E:\workbuddy\claw4-db-backup-20260913` | **pre-T1 数据库备份**（唯一回滚资产） | 回滚路径依赖 |
| `E:\workbuddy\学习习惯培育AI` | **父仓库之一**，且内含 `vendor/MetalioClaw4`（**我们 pin 死的上游只读基线**） | 搬动会让它名下 3 个 worktree 全部失效，并打断上游 pin 路径 |
| `E:\workbuddy\claw4-v53-control-20260913` | **父仓库之二**，名下 6 个 worktree | 同上 |
| `E:\c` | 上游源码**只读镜像**（`esp_claw_bin/`、`Metalio_Claw4_Latest.bin`…） | 任务书里作为**禁区引用**（`E:/c`），且 `integration_manifest.md` 记录它无 git 元数据、按只读处理 |
| `E:\b` | 基线构建产物（`xiaozhi.bin` 等） | `docs/CLAW4_PLATFORM_MAP.md` 引用了 `E:\b\xiaozhi.bin` 作为基线构建证据 |

---

## §2 git 仓库与 worktree 家族 —— 必须用 `git worktree` 命令，不能手工搬

### §2.1 三个**独立仓库**（各自有 `.git` 目录）

| 路径 | 分支 | HEAD | 状态 |
|---|---|---|---|
| `E:\claw4-a05-build-m0` | `workbuddy-a05-build-m0` | `106c500` | **本项目主仓**；工作树 clean；remote = `github.com/revercgy-hub/claw4-learning-habit-ai.git`；**7 个未推送提交**（见下） |
| `E:\workbuddy\claw4-v53-control-20260913` | `codex/v53-foundation-wave1` | `7b3df6c` | clean；名下 **6 个 worktree** |
| `E:\workbuddy\claw4-wb-v53-next-001` | `workbuddy-v53-next-001-reliability` | `19fd979` | clean；**`19fd979` 就是 A05 构建记录的 source SHA** |

`E:\claw4-a05-build-m0` 的未推送提交（远端 tip = `d5e64c5`）：

```
106c500  docs: archive project goal, current system state and reference projects（重启基线归档）
6d26014  docs(WB-A05-IDF6-001): IDF 6.0.2 迁移评估
2135fa5  docs(WB-A05-DEVICE-T1-001): 吸收两个已部署参考项目
7892f30  chore(WB-A05-DEVICE-T1-001): 隔离树重放 + 构建输入重新冻结
7ac29ad  fix(WB-A05-DEVICE-T1-001): ACK 队列卡死修复
b10dfde  docs(WB-A05-DEVICE-T1-001): 修复方案 + 主机侧复现
1a3d3fe  docs(WB-A05-DEVICE-T1-001): T1 报告 + wire 证据
```

> ⚠️ 这 7 个提交**只在本机**。任何"搬迁/清理"动作前，**先把它们推到远端**是最低成本的保险。

### §2.2 6 个 worktree（父 = `claw4-v53-control-20260913`）

| worktree | 分支 | HEAD | 已推 origin？ | 脏 |
|---|---|---|---|---|
| `E:\workbuddy\claw4-a05-cp0-review` | `codex/a05-cp0-verification` | `70738dd` | ✅ 有 tracking | 否 |
| `E:\workbuddy\claw4-a05-cp2-review` | `codex/a05-cp2-review` | `d1bb589` | ✅ 有 tracking | 否 |
| `E:\workbuddy\claw4-a05-plan-review` | `codex/a05-build-task-review` | `82337fb` | ✅ 有 tracking | 否 |
| `E:\workbuddy\claw4-v53-a01` | `codex/v53-a01-build-provenance` | `8e2baec` | ⚠️ **无 tracking** | 否 |
| `E:\workbuddy\claw4-v53-a02` | `codex/v53-a02-reset-protection` | `efb97f8` | ⚠️ **无 tracking** | 否 |
| `E:\workbuddy\claw4-v53-b01` | `codex/v53-b01-time-authority` | `9c57da8` | ⚠️ **无 tracking** | 否 |

**注册表完整** —— 这 6 个可被父仓库正常识别（`git worktree list` 全部列出）。

### §2.3 3 + 1 个 worktree（父 = `学习习惯培育AI`）

| worktree | 父仓库注册表里的名字 | 状态 |
|---|---|---|
| `E:\workbuddy\claw4-l1-ready` | ✅ 已注册 | ⚠️ `git -C` 查询时报 `fatal: not a git repository` ⇒ **linkage 已经异常** |
| `E:\workbuddy\claw4-domain-offline-stream` | ✅ 已注册 | 同上 |
| `E:\workbuddy\claw4-mvp-stream` | ✅ 已注册 | 同上 |
| `E:\workbuddy\claw4-l1-codex-review` | ❌ **不在注册表里** | **孤儿 worktree**（父仓库已不认它） |

> 这 4 个里前 3 个**都是在役资产或历史证据**（见 §1），而且 linkage 已异常。
> ⇒ **既不能手工搬、也不宜直接用 `git worktree move`**。要动必须先修父仓库的 worktree 元数据。

### §2.4 为什么"手工搬"一定坏

```text
worktree 目录里的 .git        （一个文本文件）
    └─ 内容:  gitdir: E:/<父仓库>/.git/worktrees/<名字>      ← 绝对路径

父仓库 .git/worktrees/<名字>/gitdir    （另一个文本文件）
    └─ 内容:  E:/<worktree 路径>/.git                        ← 指回来的绝对路径
```

两边**都是绝对路径**。你搬动 worktree 目录 ⇒ **第一个文件乱**；你搬动父仓库 ⇒ **第二个文件乱**。
两者都用 `Move-Item` 搬 ⇒ **双向全断**。

**正确做法只有 `git worktree move <worktree> <newpath>`**（在父仓库里执行，它会同时更新两侧指针）。
而且父仓库必须**在场且健康** —— §2.3 那 4 个恰恰不满足。

---

## §3 纯目录 —— ✅ 可安全归档 / 清理

以下都**没有 git 元数据**，是历史构建/侦察产物，未被任何在役流程引用。
**合计约 5.5 GB**（主要为旧构建目录）：

| 路径 | 体积 | 最后修改 | 是什么 |
|---|---|---|---|
| `E:\claw4-a05-19fd979` | **2,995 MB** | 09-15 | 旧 A05 构建工作区（含 `src` + `build` / `build-cp2-00{2,3,4}` / `build-envdiag-001` / `fixture` / `logs`）⇒ 已被 `E:\a05c` 取代 |
| `E:\workbuddy\claw4-idf-cold-c5-20260906` | 807 MB | 09-13 | C5 冷构建环境 |
| `E:\workbuddy\claw4-idf-cold-20260905c` | 786 MB | 09-05 | 冷构建环境 |
| `E:\workbuddy\claw4-baseline-build` | 504 MB | 08-31 | 最老的基线构建 |
| `E:\workbuddy\claw4-idf-cold-20260905b` | 384 MB | 09-05 | 冷构建环境 |
| `E:\workbuddy\claw4-idf-cold-c5-20260906-frozen-20260912` | 85 MB | 09-12 | C5 冷构建（冻结版） |
| `E:\workbuddy\claw4-l1-codex-review` | 14 MB | 09-04 | **孤儿 worktree**（父仓库注册表已无此项，见 §2.3） |
| `E:\workbuddy\claw4-a05-cp0-recon` | 8 MB | 09-15 | CP0 侦察产物 |
| `E:\claw4-a05-cp1-fix-recon` | 7 MB | 09-16 | CP1 修复侦察产物（`FALSIFIED_*` / `HEAD_*` / `commit-msg-*`） |
| `E:\workbuddy\claw4-idf-cold-20260905` | 1 MB | 09-05 | 冷构建环境 |
| `E:\workbuddy\claw4-build` | 1 MB | 08-31 | 最老的构建目录 |
| `E:\workbuddy\claw4-l1-evidence` | 1 MB | 09-04 | L1 证据产物 |

**搬迁前需注意**：`E:\claw4-a05-19fd979\src` 被 `integration_manifest.md` 以 `E:/claw4-a05-19fd979/src` 引用过
（作为"隔离重放树"历史记录）。搬走后该路径引用会失效 ⇒ 需在 manifest 里标注新位置或标 `SUPERSEDED`。

### §3.1 全盘体积总览（实测 2026-09-19）

| 分类 | 合计 | 说明 |
|---|---|---|
| 🚫 **在役资产**（§1） | ≈ **12.2 GB** | `claw4-idf-tools` **6,531 MB**、`a05c` 1,518 MB、`b` 783 MB、`c` 704 MB、`claw4-l1-ready` 456 MB、`esp-idf-5.5.4-ascii` 450 MB、`claw4-domain-offline-stream` 269 MB、`claw4-db-backup` ≈0 MB |
| ⚠️ **主仓** | 903 MB | `E:\claw4-a05-build-m0`（含 `.git`） |
| ⚠️ **父仓库 + 其 worktree** | ≈ **1,990 MB** | `claw4-wb-v53-next-001` **1,083 MB**、`claw4-v53-a02` **577 MB**、`claw4-v53-control` 154 MB、`claw4-a05-plan-review` 119 MB、其余 4 个各 2–3 MB |
| ✅ **可归档**（§3） | ≈ **5.5 GB** | 12 个纯目录 |


---

## §4 文档/脚本里被引用的 E 盘路径（动任何目录前必须一起改）

| 被引用路径 | 引用出处 |
|---|---|
| `E:\a05c\s` / `E:\a05c\b` | 构建 recipe、`run-a05-cp2-build.ps1` 默认参数、`a05_build_input_manifest.json` 的 `isolated_src.origin` |
| `E:\workbuddy\esp-idf-5.5.4-ascii` | `IDF_PATH`（构建 recipe） |
| `E:\workbuddy\claw4-idf-tools` | `IDF_TOOLS_PATH` / `IDF_PYTHON_ENV_PATH` |
| `E:\workbuddy\claw4-l1-ready\backend` | T1 报告、`T1-PRECHECK.md` |
| `E:\workbuddy\claw4-domain-offline-stream\backend\.venv` | T1 报告（共享 venv） |
| `E:\workbuddy\claw4-db-backup-20260913\...bak` | T1 报告（回滚路径） |
| `E:\claw4-a05-19fd979\src` | `integration/metalio_claw4/integration_manifest.md` |
| `E:\b\xiaozhi.bin` | `docs/CLAW4_PLATFORM_MAP.md`（基线构建证据） |
| `E:/c` | 任务书**禁区**引用 |
| `E:\workbuddy\学习习惯培育AI\vendor\MetalioClaw4` | 上游 pin |

---

## §5 推荐规整方案（三选一）

### 方案 A —— 零风险：不动任何文件
只用本文当索引。目录仍在原处，但**你知道每个是什么、能不能动**。
代价：E 盘根目录看起来仍乱。

### 方案 B —— 推荐：只搬"纯目录"，worktree 走 git 命令
1. 新建 `E:\claw4-archive\`
2. 把 §3 的**纯目录**（12 个）搬进去，按类分子目录：`archive\builds\`、`archive\recon\`、`archive\evidence\`
3. §2.2 那 6 个 review worktree：**不搬**，改用 `git worktree remove`（在父仓库执行）清理 —— git 官方方式，比搬更干净，分支仍留在父仓库
4. §1 的在役资产 + 两个父仓库 + 主仓：**原地不动**
5. 更新 §4 里受影响的引用（实际只有 `E:\claw4-a05-19fd979\src` 一处需要改）

**风险**：低。收益：清掉 E 盘根 3.1 GB 级别噪音 + 12 个目录。

### 方案 C —— 彻底：连 worktree 家族一起搬
所有 worktree 用 `git worktree move` 搬到统一父目录下。
**需要先做**：① 修 `学习习惯培育AI` 的 worktree linkage（3 个已异常、1 个孤儿）；
② 确认 `a01`/`a02`/`b01` 三个无 tracking 的分支已推送远端；③ 逐个 `git worktree move`；④ 全量更新文档引用。
**风险**：高。收益：目录更整齐 —— 但**这些 worktree 都已 clean，搬动不产生任何工程价值**。

---

## §6 执行前检查清单（任何方案都适用）

- [ ] **先把 `E:\claw4-a05-build-m0` 的 7 个未推送提交推到远端**（最低成本保险）
- [ ] 确认没有正在运行的构建 / 后端 / relay 进程占用目标目录
- [ ] 若要动 worktree：父仓库必须在场，且用 `git worktree list` 确认注册表健康
- [ ] 若要动 `E:\a05c`：**先确认新路径长度不会撞 32,767 命令行上限**，并重新生成 `a05_build_input_manifest.json`
- [ ] 搬迁后逐项复验：`git -C <repo> status` 干净、`git worktree list` 完整、`verify-build-inputs.py --check` PASS
