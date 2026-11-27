# CycloneDDS 源码

本目录仅随包交付 CycloneDDS，不包含 GStreamer。

- 版本：11.0.1。
- 来源、原始压缩包 SHA-256 和许可证标识：见 [manifest.json](manifest.json)。
- 上游源码：已解压到 [cyclonedds-11.0.1](cyclonedds-11.0.1/)，保留原始字节；本项目没有修改上游源码。
- 文件校验清单：[SHA256SUMS](SHA256SUMS)，共 1,507 个文件。
- 许可证原文：[LICENSE](cyclonedds-11.0.1/LICENSE)、[NOTICE.md](cyclonedds-11.0.1/NOTICE.md)。

在 SDK 根目录执行：

```sh
python3 deploy/dds.py verify-sources
bash deploy/deploy.sh --build-only --skip-packages
```

部署脚本使用上游 CMake 将静态 DDS 构建到 SDK 根目录的 `.deps/<工具链标识>/`，不在本目录生成对象文件，也不安装系统 DDS。普通 SDK `make` 不构建本目录。

`--skip-packages` 适用于客户已准备工具链、Python ≥ 3.8、Make、CMake ≥ 3.16、pkg-config、GStreamer ≥ 1.20 开发包及运行插件的情况。只有 DDS 源码随包提供；其他依赖缺少时仍需自行准备或允许部署脚本安装。

交叉编译、缓存和客户工程链接方法见[依赖部署与交叉编译](../doc/依赖部署与交叉编译.md)。
