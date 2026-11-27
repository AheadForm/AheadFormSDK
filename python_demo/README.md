# Python 接入示例

通过 CFFI 编译模式复用现有 C API 和可选 User 业务，不维护另一套 SDK 实现。默认部署只准备项目私有 Python/CFFI 环境；API/User 扩展由客户在本目录手动 make 构建；普通 C/C++ 的 `make` 仍不构建 Python 接入。

Python 是可选项：CFFI 安装失败时继续 C/C++ 依赖部署，脚本最后再次显示两种环境的准备状态及失败原因。Python 环境准备成功后仍需先 make，再运行示例；失败时补齐依赖、重跑部署即可。主动中断仍取消部署，独立执行本目录 `make` 遇到错误仍报错退出。

首次在 SDK 根目录部署，之后进入本目录手动构建并运行：

```sh
bash deploy/deploy.sh                   # 在 SDK 根目录执行一次，无需机器人参数
# 按目标机器人修改 CodeProject/AheadFormSDK_Config.ini 后再运行业务指令。
cd python_demo
make                                   # 首次使用也需要：构建 API/User 扩展
.venv/bin/python demo_user.py --list    # 列出指令，不初始化机器人
.venv/bin/python demo_user.py           # 进入 >> 交互菜单，输入 exit 退出
.venv/bin/python demo_user.py QueryVolume # 保留单次指令用法
make api                               # 修改代码后：仅构建 API 接入
make                                   # 修改代码后：构建 API 和 User 接入
```

交互模式与 C 入口保持相同顺序：先初始化 SDK，成功后才显示输入说明和 `>>`，输入 `help` 查看原 User 菜单。后续指令复用同一实例，退出时释放；初始化失败直接报错退出，不显示菜单或接受输入。单独使用 `--help`、`--list` 或单次 `help`/`exit` 不初始化机器人。启动交互模式前须核对配置、网络和机器人状态。

部署结尾分别给出 C/C++ 客户和 Python 客户的用法，按使用语言选择一项。Python make 末尾列出 Python 扩展的完整路径及可整行复制的运行命令；标准 API 动态库需要重建时，由 API 子 Makefile 单独报告一次完整路径。默认保留配置，也可选填机器人参数自动配置；切换机器人时修改 INI 并重启 Python 程序即可，无需再次部署。可通过 `--config /实际路径/config.ini` 为示例指定其他配置。

`make` 默认优先使用本目录 `.venv/bin/python`，仍可显式指定 `PYTHON`。只需 C/C++ 时，部署加 `--skip-python`；离线 Python 包使用 `--python-wheelhouse /包目录`。日常修改源码或绑定声明后重新 `make` 即可，无需重复部署。更换工具链或编译参数并重新部署后，在本目录执行 `make clean && make`；clean 会同时清理标准 API 库及对象文件，保留私有 DDS 和 Python 环境。详细用法、离线安装、交叉编译、生命周期和边界见 [Python 接入说明](doc/Python接入说明.md)，实际测试见[验证记录](doc/验证记录.md)。SDK 总入口见[文档与代码清单](../doc/文档与代码清单.md)。
