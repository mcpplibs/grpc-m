# grpc-m

gRPC **1.83.0** 的 C++23 模块封装，保留 `grpc::` API，并统一 protobuf 代码生成工具。

[English](README.md)

## 快速开始

使用 mcpp **2026.10.24.1**。以下为候选版本，索引发布后可直接创建项目。

```sh
mcpp new greeter --template grpc.grpc@1.83.0.1
cd greeter
mcpp run
```

默认模板生成 stub、启动本地回环服务器，并验证真实 RPC 响应。

## 已有项目

```toml
[dependencies]
"grpc.grpc" = { version = "1.83.0.1", features = ["codegen"] }
```

把 schema 放入 `proto/helloworld.proto`，并添加 `build.mcpp`：

```cpp
import mcpp;
import grpcgen;
int main() { return grpcgen::generate_all() ? 0 : 1; }
```

```cpp
#include "helloworld.grpc.pb.h"
import grpc;
int main() { return grpc::Status::OK.ok() ? 0 : 1; }
```

文本 include 放在 import 之前。模板已经提供代码生成所需的 build 程序。
`codegen` 提供配套 protoc、C++ plugin 与规则；把自己的 schema 放入 `proto/` 即可。

## 能力边界

当前依赖集支持 Linux 与 macOS，Windows 暂未提供。
默认 `ares` 使用 c-ares；`default-features = false` 改用原生解析器。
交叉编译应用时，生成工具仍为构建机编译。

## 文档

[用法与代码生成](docs/usage.zh.md) · [Greeter](examples/greeter) ·
[高级代码生成](examples/advanced) · [Apache-2.0](LICENSE)
