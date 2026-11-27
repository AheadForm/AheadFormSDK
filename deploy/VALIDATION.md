# 私有 DDS 部署验证记录

当前部署行为以末尾“依赖部署与手动构建分离”一节为准；此前章节保留对应版本的历史验证结果。

日期：2026-11-26。对应本次 main 工作区的离线 DDS 与私有部署改动。

## 构建与运行验证

| 环境 | 工具链 | 实际结果 |
| --- | --- | --- |
| Ubuntu 22.04 x86_64 虚拟机 | GCC 11.4 | 部署、主程序、独立 API 库、参数查询、客户程序链接及普通增量构建通过 |
| 禁网 Linux 容器，x86_64 | GCC 11.4 | 冷缓存 DDS / 主程序 / API 构建通过 |
| 禁网 Linux 容器，x86_64 | Clang 14.0 | 冷缓存 DDS / 主程序 / API 构建通过 |
| 禁网 Linux 容器，ARM32 hard-float | arm-linux-gnueabihf GCC 11.4 | 冷缓存交叉构建及外部 API 链接通过；ELF32 ARM |
| 禁网 Linux 容器，ARM64 | aarch64-linux-gnu GCC 11.4 | 冷缓存交叉构建及外部 API 链接通过；ELF64 AArch64 |
| ARM64 目标 sysroot，禁网容器 | aarch64-linux-gnu GCC 11.4 | 使用目标 GStreamer 开发包完成主程序及独立 API 构建、客户程序链接，核对 CMake 和 SDK 的 sysroot 参数 |
| Debian 12 最小容器 | GCC 12.2 / APT | 自动安装缺少的工具和 GStreamer，完成私有 DDS / SDK 构建；最终脚本重复部署通过 |
| Fedora 43 最小容器 | GCC 15.3 / DNF | 自动安装缺少的工具和 GStreamer，完成私有 DDS / SDK 构建；最终脚本重复部署通过 |
| openSUSE Tumbleweed 容器 | GCC 16.2 / Zypper | 初次发现缺少 findutils，补齐部署检查和包映射后通过；最终脚本重复部署通过 |

禁网构建使用 `--network none`，构建环境没有系统 CycloneDDS 开发包；客户工具链及匹配的 GStreamer 在测试前准备。已核对 DDS 使用随包源码重新构建，最终链接路径指向当前项目的 `libddsc.a`，主程序没有 `libddsc.so` 依赖。

GCC、Clang、ARM32、ARM64 四组均运行了以下探针；ARM 使用 QEMU 用户态执行：

- 仅在容器回环接口创建、释放 DDS domain 和 participant。
- 从测试指定的 GStreamer 环境加载 18 个必需元素。
- PCM → RTP/L16 → PCM 的 640 字节往返比对。
- 调用 API 解码示例 MP3 和临时 WAV，核对 16 kHz 单声道输出。
- 独立 API 头文件、编译参数及完整链接参数的实际使用。

Ubuntu 虚拟机也运行了回环 DDS 和媒体 API 探针。没有启动机器人业务流程、播放机器人动作或发送机器人控制指令。

## 构建状态与失败路径

已验证：

- 普通重复 make 不重编对象、不构建 DDS。
- 重复部署复用成功 DDS，归档内容与时间戳不变。
- 未部署时两个入口均能 clean；普通构建明确拒绝回退到系统 DDS。
- 编译器或参数与选中缓存不匹配时，在编译前报错。
- 私有归档损坏、源码校验失败会被拒绝。
- 注入 DDS 构建失败后无成功标记，移除故障后可重试。
- 注入 SDK 编译失败后清除活动选择，防止旧配置与新对象混用。
- User 多级目录新增源码和头文件可自动收集，修改头文件会重编；删除源码按文档 clean 后不残留符号。
- 将工程移动到另一目录，并使旧路径不可访问后，普通构建和 DDS 缓存复用通过。
- 默认主机 pkg-config 和与目标架构不匹配的 GStreamer 头文件/库会被拒绝。
- 自带固定目标路径的 pkg-config 包装器只用于 GStreamer，不能覆盖私有 DDS 查询；交叉模式自动跳过机器人配置。
- 此前离线 DDS 与私有部署阶段的 10 项逻辑测试通过，涵盖配置注释和 CRLF 保留、字段缺失、发行版分组、Arch 检查策略、跳过包操作、工具链推断、私有查询与无效参数提前报错。

原 Ubuntu 虚拟机没有执行包安装。对原系统 DDS 库 / .pc、全局环境文件及动态库配置做了构建前后哈希比对，结果一致；项目 INI 也逐字节一致。系统包安装测试均在隔离容器中完成。

## 交互与中断恢复验证（2026-11-26）

本轮新增的回归测试位于 [tests/test_deploy.py](tests/test_deploy.py)，在 Ubuntu 虚拟机执行：

```sh
python3 -m unittest discover -s deploy/tests -v
```

29 项测试全部通过，包含一次真实的 30 秒等待：

- 伪终端中的回车、修改、可选项清空、无效输入重试、超时、未提交半行丢弃、Ctrl+C 和 EOF；无有效默认值超时退出。
- 本机构建只问 C 编译器；GCC、Clang 及 ARM 工具链配套工具推导，客户显式覆盖，未知编译器补问而不使用无关主机工具。
- 更换编译器、sysroot 或编译参数时重新确定目标依赖；保留本次明确提供的配置。交叉包装脚本不重复询问内部 .pc 目录。
- 非交互跳过提问、旧活动配置迁移、项目锁释放和互斥、成功设置的原子保存。
- INI 注释、CRLF 和权限保留；原子替换中断后原文件不变，重试成功，内容相同时不再备份或写入。
- DDS 重建先取消成功标记；构建中断会终止子进程及孙进程。

另在临时源码副本分别向 DDS 配置、编译、安装和 SDK 编译、链接阶段发送 SIGTERM，五种情形均可重跑成功。DDS 未完成时没有成功标记；SDK 未完成时没有活动依赖选择；上次成功设置保持不变。实际并发启动第二个部署进程会被拒绝。完整 CLI 的伪终端回车与 EOF 也已验证。

最终脚本在禁网容器中复测 GCC、Clang、ARM32、ARM64 的主程序、独立 API、参数查询、客户程序链接和普通增量构建，全部通过。冷缓存构建及后续缓存复用均通过；最终 ELF 不依赖 `libddsc.so`。交叉环境预先准备了匹配的目标 GStreamer，没有使用主机 GStreamer 替代目标依赖。

原 Ubuntu 虚拟机再次以 `--build-only --skip-packages --non-interactive` 部署成功，DDS 静态库内容和修改时间不变。本轮未安装或升级虚拟机系统包，未运行机器人业务指令。

系统包中断行为使用模拟事务验证，覆盖 Python 安装入口的 Bash 保护和 Python 部署逻辑中的 SIGINT/SIGTERM 等待退出。没有为测试而中断真实包数据库事务，不能据此保证强杀或断电后的系统包数据库自动恢复。

本轮逐字节核对了 52 份业务源码、头文件、IDL 和 INI，以及两个 Makefile、`deploy/deps.mk`；均与本轮修改前一致。Git 索引和 HEAD 也保持原样。

本轮日志和临时副本保留于 `/tmp/aheadform-interactive-validation/`；回归测试日志为 `/tmp/aheadform-interactive-tests.log`，虚拟机重复部署日志为 `/tmp/aheadform-interactive-native-final.log`。

## 文件与交付核对

- 1,507 个上游源码文件与 SHA-256 清单一致，保留原始字节。
- 第三方源码中未放入本次构建产生的 .so / .a；产物位于忽略的 .deps。
- Python 语法、Shell 语法、文档本地链接和 git diff --check 已核对。
- main 保持未提交；Test 原工作状态保留于外部备份和 stash。未推送任何远端。

本机详细日志与临时探针保留于 `/tmp/aheadform-main-private-dds-validation/`。

## 验证边界

以上交叉运行使用 QEMU，未代替真实 ARM 板上的驱动、网络及媒体验证。没有覆盖每个发行版、厂商工具链、glibc 版本或所有 GStreamer 升级组合；旧发行版仓库可能无法提供最低版本或必要插件。GStreamer 仍依赖客户环境，只有 DDS 源码随包离线提供。

## 默认 C/C++/Python 部署验证（2026-11-27）

在 Ubuntu 22.04 x86_64 隔离容器及 `/tmp` SDK 副本验证，未安装或升级原虚拟机系统包，也未初始化机器人：

- 从缺少 venv/ensurepip 的环境默认部署，APT 自动补齐 Python 开发文件、venv/pip 及 libffi 开发文件；在项目 `.venv` 安装 CFFI，成功生成 C/C++ 主程序、标准 API 动态库和两个 CFFI 扩展。
- 系统 Python 在部署后仍无法导入 CFFI，确认 Python 包只安装在项目私有环境；本次 `.venv` 约 21 MiB、Python `.build` 约 908 KiB，不包含系统开发包及 DDS/API 产物，不代表其他发行版固定占用。
- 断开容器网络，重新创建私有环境，通过 `--skip-packages --python-wheelhouse` 从预备的 CFFI/pycparser wheel 完成全部构建。随后无 wheelhouse 重复部署，已有 Python 包检查通过，未调用 pip。
- 不激活虚拟环境、不显式指定 `PYTHON`，直接 `make -C python_demo -j4` 通过，重复构建没有编译或链接。
- 暂时移走私有 Python 环境后，以 `--skip-python --skip-packages` 完成纯 C/C++ 部署；没有创建 `.venv` 或生成 API `.so`。恢复环境后默认部署再次通过。
- Python 接入原有 15 项替身测试通过；部署回归共 40 项通过，新增覆盖私有环境复用、未完成环境修复、pip 失败重试、离线参数、宿主工具链隔离、Python 构建中断不发布成功设置及交叉构建不运行目标扩展。构建配置比较忽略参数间无意义空白，同时保留引号内的实际内容变化，避免部署后首次普通 make 无谓重编译。
- 真实 SDK 只加载、列出 17 条 User 指令，INI 内容保持不变；API 动态库无 `libddsc.so` 依赖。Shell/Python 语法及 `git diff --check` 通过。

新 Python 系统包映射本轮实际验证 APT；DNF/Zypper 自动安装及 ARM32/ARM64 的完整 Python 交叉构建未在本轮实测。交叉模式仍必须提供目标 Python 开发配置，不能用以前的 C/C++ 交叉验证替代 Python 验证。日志前缀为 `/tmp/aheadform-python-default-`，构建副本位置记录于 `/tmp/aheadform-python-default-current`。

## Python 失败时继续 C/C++ 部署（2026-11-27）

- Python 准备移至 DDS 和 C/C++ 主程序构建、链接检查之后。Python 环境准备、CFFI 安装、扩展构建或加载检查失败时继续核心部署，核心步骤成功时退出码为 0；最后再次显示 C/C++ 成功、Python 未就绪、失败原因及重试方式。
- 在 Ubuntu 虚拟机的临时 SDK 副本中，使用带 pip 但没有 CFFI 的私有环境及空离线 wheelhouse，实际复现 CFFI 安装失败：C/C++ ELF 和活动依赖配置仍生成，退出码为 0，最后的结果提示完整。
- 在临时副本的 CFFI 生成头部加入 `#error`，实际复现扩展编译失败，同样退出 0 且 C/C++ 可用；删除注入后重跑，API/User 扩展构建、加载和 17 条命令枚举成功。两种失败均清除 Python 活动选择标记，保留虚拟环境及缓存。
- 部署回归共 46 项通过，覆盖环境/安装/编译/加载失败的降级、无效离线目录、交叉构建缺少目标 Python 配置、重试恢复、最终输出重复具体原因，以及显式跳过 Python。主动中断仍取消部署，不被降级逻辑吞掉；C/C++ 核心失败仍报错退出。
- 原有 15 项 Python 替身测试通过。未修改原虚拟机系统包、业务源码、公开 API 或机器人配置，也未运行机器人业务指令。跨发行版与 ARM Python 实机运行仍沿用上述未验证边界。

本轮日志为 `/tmp/aheadform-python-fallback-{pip,build,retry,tests,binding-tests}.log`，所有实际构建在既有 `/tmp` 副本中执行。

## 机器人参数可选部署验证（2026-11-27）

- 在 Ubuntu 虚拟机的当前 SDK 路径，通过伪终端实际执行不带参数的 `bash deploy/deploy.sh`，在 C 编译器提示处回车。C/C++ 主程序、API 动态库及 Python API/User 扩展全部构建和加载成功，没有要求机器人参数。
- 默认部署前后逐字节核对 `CodeProject/AheadFormSDK_Config.ini`，内容及备份文件清单未变。私有 DDS 静态库的哈希与修改时间未变，确认复用缓存。末尾打印配置文件完整路径、网卡、外部 DDS 域、机器人 IP 及媒体配置提醒。
- 部署回归共 52 项通过，新增覆盖缺少网络工具时默认部署不安装网络包、机器人参数独立更新（含 DDS 域 0）、省略字段保持原值、配置重复更新不写入、不产生额外备份，以及无效 IP/网卡拒绝。默认部署仍尝试 Python；`--build-only` 和交叉编译仍跳过自动机器人配置。
- 本轮没有执行机器人业务指令，也没有修改业务源码、公开 API、两个 Makefile 或机器人 INI。部署与 Python 文档同步说明不带参数的默认用法、可选自动配置及切换机器人无需重新部署。

回归日志：`/tmp/aheadform-optional-robot-tests.log`；实际默认部署日志：SDK 目录下 `.deps/optional-robot-deployment.log`。

## 依赖部署与手动构建分离（2026-11-27）

- 部署只准备工具、GStreamer、私有 DDS 与可选 Python/CFFI，不清理或构建 SDK 主程序、API 库、Python 扩展，也不执行菜单枚举。DDS 仍在部署时通过上游 CMake 构建；普通 make 不构建 DDS。
- 编译器询问等待改为每项 2 分钟。53 项部署回归通过，包含一次真实 120 秒等待、超时清除未提交半行输入、配置保存及后续 Makefile 读取、无 SDK 构建/菜单调用、Python 准备失败降级和可选机器人配置。
- 在当前 Ubuntu 虚拟机交互运行无参部署，回车接受当前工具链。部署前后核对 114 个 SDK/Python 产物及配置文件，SHA-256 与修改时间均未变；没有更新机器人 INI，也没有新增备份。随后手动 `make -C CodeProject -j4`、`make -C python_demo -j4` 以及不初始化机器人的 Python 加载/命令枚举通过，主程序不依赖 `libddsc.so`。
- 在临时源码副本中通过终端输入 `gcc`，从冷缓存构建 DDS 并保存 `.deps/<id>/config.mk`、`.deps/active.mk`。部署后没有 SDK ELF、API 库、对象文件或 Python 扩展；随后手动构建 C/C++ 和 Python API/User 并加载通过。
- 在该副本的 `deploy/deps.mk` 顶部增加 `CC := gcc -g`，非交互部署读取新设置，自动推导配套 C++ 命令、生成独立 DDS 缓存并更新本地 `.mk`。用户输入与共享参数文件都可用于之后的手动构建，不会重写两个 SDK Makefile。
- 本轮当前虚拟机没有安装 Clang；选择 Clang 并指定 `--skip-packages` 时正确报告缺少工具。本轮实际冷缓存构建使用 GCC，未重做其他发行版或 ARM 交叉构建，不将历史验证当作本轮新增验证。
- 未执行机器人业务指令，未改业务代码、公开 API、两个 SDK Makefile或特点说明 Word 文档。脚本语法、文档相对链接及 `git diff --check` 通过；未暂存、提交或推送。

回归日志：`/tmp/aheadform-prepare-only-tests.log`。当前目录实测日志：`.deps/prepare-only-deployment.log`。临时副本路径记录：`/tmp/aheadform-prepare-only-current`；冷部署、手动构建和项目覆盖项日志分别为 `/tmp/aheadform-prepare-only-cold.log`、`/tmp/aheadform-prepare-only-cold-manual.log`、`/tmp/aheadform-prepare-only-project-override.log`。

## 精简部署与构建提示（2026-11-27）

- 部署结尾保留“编译、配置文件修改、SDK 运行方法”三部分；Python 准备失败仍在编译部分再次说明原因和 C/C++ 就绪状态。默认不打印清理要求、缓存目录或“本次未构建”。
- Python 默认/API/User 构建后打印实际产物绝对路径和对应运行命令；重复 make 无需重编时仍显示提示。真实 SDK 仅加载、枚举和帮助检查，不初始化机器人。
- 已按要求撤回本轮新增的 C/API 编译配置记录、比较脚本入口及其测试，两个 SDK Makefile 恢复原有规则。普通源码和头文件修改仍直接 make；更换工具链或编译参数后，由客户手动 clean 再构建。部署和 Python 提示修改继续保留。
- 撤回后在临时副本通过主程序冷构建、独立 API、Python API/User 构建及重复增量构建；标准 Python 产物路径和运行提示正常，加载/命令枚举通过。52 项部署回归通过，未重复未变更的真实 120 秒超时测试。回归日志：`/tmp/aheadform-revert-build-settings-tests.log`；构建日志：`/tmp/aheadform-restored-*.log`。

## 部署结果提示（2026-11-27）

- 部署完成先显示“部署结果：”，明确 C/C++ 依赖准备结果，以及 Python/CFFI 的成功、失败或跳过状态和具体失败原因；随后显示“后续操作指南：”，将编译、运行前配置检查和 SDK 运行方法作为指南的 1、2、3 项。交叉编译时标明 Python 状态只针对宿主环境。
- 回归覆盖完整成功、Python 可选失败后重试、显式跳过及交叉编译状态。此提示只确认依赖环境准备，不表示 SDK 或 Python 扩展已构建。
- 本轮 18 项 Python 部署测试通过；Ubuntu 虚拟机执行 `bash deploy/deploy.sh --non-interactive --skip-packages` 返回 0，确认结果和指南标题各出现一次且顺序正确。部署前后 SDK 对象、库、可执行文件及机器人 INI 的哈希和修改时间不变。实际输出记录：`/tmp/aheadform-deploy-result-guide.log`。

## 客户语言与依赖信息提示（2026-11-27）

- 结尾按客户语言给出二选一的编译与运行命令，使用 `cd ... && ...`，删除“修改后重新部署”；运行前配置板块只给出检查提醒和 INI 路径，不再列字段。
- 显示实际 CycloneDDS、GStreamer Core/App 版本及四组头文件/链接参数，GStreamer .pc 目录和已设置的 PKG_CONFIG 配置。DDS 明确标为 SDK 私有静态库。
- 当前 Ubuntu 虚拟机无包安装、无机器人参数部署通过；实际显示 DDS 11.0.1、GStreamer Core 1.20.3 / App 1.20.1。部署前后 SDK/Python 对象、库、ELF 和机器人 INI 的哈希及修改时间不变。
- 52 项部署回归通过，未重复未改动的真实 120 秒等待测试。临时副本冷构建、重复构建、API 独立入口及 Python 加载/枚举通过；成功产物路径不重复打印，显示的运行命令追加 --list/--help 后可从其他目录直接执行，不初始化机器人。
- 临时复制现有 GStreamer .pc 文件并通过 PKG_CONFIG_PATH 指定后，报告正确显示所选目录及参数；没有改变系统库或 DDS 选择。原编译流程不变，API 子 Makefile 仅将动态库成功提示改为绝对路径。

日志：`/tmp/aheadform-customer-output-tests.log`、`.deps/customer-output-deploy.log`、`/tmp/aheadform-customer-python-{cold,repeat,api}.log`。

## 工具链与依赖信息集中显示（2026-11-27）

- 标题改为 `Selected toolchain and deploy:`，依次显示 CC/CXX/AR/RANLIB、私有 DDS 与 GStreamer 的版本、头文件/链接参数、.pc 目录及已设置的 PKG_CONFIG 参数，再准备 Python 环境。末尾三个使用板块不重复依赖详情。
- deps.mk 中可选 sysroot、编译参数和 pkg-config 示例改用 `?=`，保留环境变量优先级；CC/CXX 示例保留 `:=` 并注明 GNU Make 已有内建默认值。新增验证可选设置的默认值和环境变量覆盖。
- 53 项部署回归通过，未重复未改动的真实 120 秒超时测试；包括依赖详情顺序、只显示一次，以及原有 Python 失败降级、中断和配置保存。
- 当前 Ubuntu 虚拟机以 `--non-interactive --skip-packages` 实际部署通过，复用 DDS 和私有 CFFI；核对 106 个 SDK/Python 产物与机器人 INI 的哈希和修改时间均未改变。未安装系统包、未构建 SDK、未执行机器人业务指令。

日志：`/tmp/aheadform-deploy-selection-output-tests.log`、`.deps/selection-and-deploy-output.log`。

## 编译器保存与清理提醒（2026-11-27）

- 保留现有工具链持久化机制：部署把确认的参数写入 `.deps/<id>/config.mk`，通过 `active.mk` 和 `deploy/deps.mk` 供两个 Makefile 自动读取，不需要下次重新输入，也不重写源码 Makefile。
- 部署末尾“编译”栏增加实际保存路径，并提示修改编译器或编译参数后先 clean 再构建。每种客户只需清理自己的构建入口；Python 的 clean 同时调用标准 API 清理，普通源码修改仍直接 make。
- 配置保存/重新读取和最终输出两项针对性回归通过。虚拟机显式设置 CC 部署后，在不传任何工具链环境变量的新进程中，分别预览总 Makefile 和独立 API 构建，确认都使用保存的编译器；106 个已有产物及机器人 INI 的哈希与修改时间未变。
- 本轮未构建 SDK、未安装系统包、未操作机器人。实测日志：`.deps/compiler-saved-and-clean-hint.log`。

## Python 客户单目录清理（2026-11-27）

部署的清理提醒简化为“修改编译器或编译参数后，请在对应构建目录先执行 make clean，再执行 make”。Python 客户只需在 python_demo 操作；该 Makefile 的 clean 已调用标准 API 清理，不再要求客户到两个目录手动清理。

最终输出针对性回归通过。临时副本实际验证 Python/API 清理、无 Python/DDS 配置清理、重建和重复增量构建，DDS 缓存及虚拟环境保留，原工作区 API 文件未变；未初始化机器人。日志：`/tmp/aheadform-python-clean-validation.log`。

## 客户配置入口与内部路径（2026-11-27）

默认输出移除“.pc 目录”和“工具链已保存：config.mk”两项，客户编译配置入口只显示 deploy/deps.mk；依赖版本、编译链接参数和客户设置的 PKG_CONFIG 参数仍保留。config.mk/active.mk 继续由部署脚本自动保存和读取，客户无需手改，参数持久化机制不变。

配置保存读取与最终输出两项针对性回归通过；当前虚拟机实际部署确认这两项内部路径不再出现，106 个 SDK/Python 产物及机器人 INI 的哈希和修改时间不变。未构建 SDK、未安装系统包、未初始化机器人。日志：`.deps/customer-config-entry-output.log`。
