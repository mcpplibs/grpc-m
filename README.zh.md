# grpc-m

> 从源码构建 gRPC 1.83.0，保留 grpc::，共享索引依赖。

[English](README.md) · **简体中文** · [Upstream](https://github.com/grpc/grpc) · [Releases](https://github.com/mcpplibs/grpc-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache-2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/grpc-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/grpc-m/actions/workflows/ci.yml)

## 模块

- `import mcpplibs.mwrap.grpc;`: grpc:: 客户端、服务端与状态 API
- `import mcpplibs.mwrap.grpc.macros;`: 版本宏映射

> More Modules API -> [模块与 API](docs/usage.zh.md)

## 快速开始

```sh
mcpp new greeter --template mcpplibs.mwrap.grpc@1.83.0
cd greeter
mcpp run
```

```toml
[dependencies]
mcpplibs.mwrap.grpc = { version = "1.83.0", features = ["codegen"] }
```

## 两种使用方式

```cpp
#include "helloworld.grpc.pb.h"
import mcpplibs.mwrap.grpc;
```

标准库与生成头放在 import 前；也可直接使用 `<grpcpp/grpcpp.h>`。
`.macros` 映射版本常量；预处理宏仍通过文本头使用。

## 代码生成

`codegen` 提供 protoc、grpc_cpp_plugin 与宿主规则；默认模板实际生成存根。
[规则 API 与生成头顺序](docs/usage.zh.md)

## 需要更多控制时

使用 `grpcgen::generate_all`、`plan_all`、`submit` 配置导入目录、插件与显式构建边。

## 平台支持

运行时支持 Linux/macOS；当前 OpenSSL 供应者未支持 Windows 运行时，宿主工具单独测试。

## 特性

| Feature | 用途 |
|---|---|
| `codegen` | protoc、C++ 插件与宿主规则 |
| `ares` | c-ares DNS 解析器 |

## 构建方式

固定 gRPC 源码配合索引依赖，私有配置位于 `build.mcpp`；见[详情](docs/usage.zh.md)。

## 本地构建与测试

```sh
mcpp build
mcpp test
```

## 许可证

[Apache-2.0](LICENSE)；保留第三方依赖通知。

封装：Apache-2.0，mcpp-community。[上游原始 README 与许可证](upstream/grpc/)，上游许可保持原样。
