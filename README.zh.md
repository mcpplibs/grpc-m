# grpc-m

> 面向 mcpp 的 gRPC 1.83.0 —— 从上游源码构建、`import grpc;` 即用、一条命令跑通一次真实 RPC

[![Release](https://img.shields.io/github/v/release/mcpplibs/grpc-m)](https://github.com/mcpplibs/grpc-m/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Module](https://img.shields.io/badge/module-ok-green.svg)](https://en.cppreference.com/w/cpp/language/modules)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)
[![gRPC 1.83.0](https://img.shields.io/badge/gRPC-1.83.0-brightgreen.svg)](https://github.com/grpc/grpc/releases/tag/v1.83.0)

| [English](README.md) - 简体中文 |
|:---:|
| [mcpp 构建工具](https://github.com/mcpp-community/mcpp) · [包索引](https://github.com/mcpplibs/mcpp-index) · [Issues](https://github.com/mcpplibs/grpc-m/issues) |

`grpc-m` 从上游源码构建 gRPC 的 C++ 栈,**零补丁**;abseil、protobuf、re2、c-ares、OpenSSL、zlib
全部取自 [mcpp-index](https://github.com/mcpplibs/mcpp-index),而不是各自再 vendor 一份。
无 CMake、无 Bazel、无 configure —— `mcpp build` 就是全部。

## 快速开始

```bash
mcpp new mygreeter --template grpc && cd mygreeter
mcpp run
```

```
server listening on 127.0.0.1:34733
Greeter replied: Hello mcpp
```

或加入已有工程:

```bash
mcpp add grpc.grpc
```

```toml
[dependencies.grpc]
grpc = "1.83.0"
```

## 两种用法

gRPC 的代码生成器产出的是**头文件**,所以任何真实程序都会 include protoc 的产物。模块层是
你手写的那部分(服务端、通道、凭据)的更好写法。

```cpp
#include <string>                     // 标准库头在前
#include "helloworld.grpc.pb.h"       // 然后是 protoc 产物
import grpc;                          // 模块放最后 —— 原因见下

class Greeter final : public helloworld::Greeter::Service {
    grpc::Status SayHello(grpc::ServerContext*, const helloworld::HelloRequest* req,
                          helloworld::HelloReply* rep) override {
        rep->set_message("Hello " + req->name());
        return grpc::Status::OK;
    }
};
```

> **`import grpc;` 必须放最后。** 该模块的 BMI 里带着标准库(它包裹 `<grpcpp/grpcpp.h>`,
> 而后者会拉进大半个标准库),所以在 import **之后**再 textual include 会让同一批声明到达两次,
> 构建直接失败 —— 报 `redefinition of std::__is_constant_evaluated`,或 `std::string` 上的
> `ambiguous overload for operator==`。把所有 `#include` 放在 import 之前就一切正常:
> 导出的实体属于**全局模块**,textual 视图与 import 视图是同一批实体。两种顺序均已用
> gcc 16.1.0 实测验证。

也可以完全不用模块,直接走头文件:

```cpp
#include <grpcpp/grpcpp.h>
#include "helloworld.grpc.pb.h"
```

## 代码生成

gRPC 需要两个宿主工具 —— `protoc` 与 `grpc_cpp_plugin` —— 自 **mcpp 2026.8.5.1** 起
你不再需要自己准备它们,也不需要知道它们是谁:

```toml
[dependencies.grpc]
grpc = { version = "1.83.0", features = ["codegen"] }
```

```cpp
// build.mcpp —— 全文如此
import mcpp;
import grpcgen;
int main() { return grpcgen::generate_all() ? 0 : 1; }
```

一条依赖,一行代码。gRPC 的代码生成需要哪些工具 —— protoc、C++ 插件、驱动它们
的那条规则 —— 是**这个包**的知识,`features = ["codegen"]` 就是它说出这件事的
地方。新增一个 `.proto` 只是往 `proto/` 里放一个文件,没有需要同步维护的清单。

这就是 `templates/greeter`,`mcpp new --template grpc` 直接给你一份能建的
(`--template` 接的是**包名**;一个包带多个模板时写成 `grpc:greeter`)。
**仓库里不再签入任何生成产物** —— 改 `.proto` 然后重新构建,就这样。

`grpcgen` 是一个普通的 mcpp 包,里面装着那条规则,用 C++ 写、与 gRPC 同版本发布。
规则走你已经在用的包管理器分发,所以这里**没有第二门语言** —— 不像 xmake 用 Lua rule、
Bazel 用 Starlark。它需要 **mcpp 2026.8.6.2**;2026.8.5.2 是规则包从那一版起才真正能用
`import std;` 与 `import mcpp;` 的。

## 需要更多控制时

`generate_all()` 之外不是断崖 —— 每一层都是下一层的默认值,**不是另一条路**:
`generate_all(opt)` 就是 `submit(plan_all(opt))`,`.grpc = true` 就是
`.plugins = {cpp()}`。

```cpp
// L1 —— 声明式旋钮
grpcgen::generate_all({
    .extra_dirs  = {"../shared-proto"},   // 也要生成的共享 .proto 树
    .imports     = {"/opt/googleapis"},   // 只搜索,不生成
    .mock        = true,                  // gRPC 的 generate_mock_code=true
    .protoc_args = {"--experimental_allow_proto3_optional"},
});

// L2 —— 插件列表;`.grpc = true` 是「列表里有 cpp()」的语法糖
grpcgen::generate_all({ .plugins = { grpcgen::cpp(), my_plugin } });

// L3 —— plan / submit 分离:再离谱的需求也不必绕开规则
auto edges = grpcgen::plan_all();
for (auto& e : edges) e.arg("--whatever");
grpcgen::submit(edges);
```

`extra_dirs` 与 `imports` 的区别不是学究:protoc 会把
`#include "common/types.pb.h"` 写进任何 import 了它的文件,所以只靠 `-I` 够到的
共享树会产出一个**没人生成**的头,报错还离原因很远。代码归你构建就用
`extra_dirs`,代码来自别处(你已经链接的包)才用 `imports`。

**可观测性的分工**:每条边的完整命令行由 mcpp 写进 `build.ninja`,
`ninja -t commands <output>` 就能取 —— 规则不重造。规则负责的是「**哪些旋钮**产生
了它」,写在每次构建都会打印的 description 里:

```
GENERATE protoc:orders (+grpc +mock, -Iproto -I../shared-proto)
```

`examples/advanced` 覆盖 L1 + L3。

三个示例,是刻意的:

| | |
|---|---|
| `examples/greeter` | 模板的实例化 —— 经 `grpcgen`,`build.mcpp` 三行 |
| `examples/helloworld` | 同一个程序,但把规则手工摊开写,让机制保持可读 |
| `examples/advanced` | 分层旋钮:跨根生成的共享 .proto 树、gRPC mock、plan/submit 中间加断言 |

由此得到三个手工管理 codegen 给不了的性质:

- **版本错配不可表达。** 工具的版本**就是**那条依赖的版本,所以「protoc 与 protobuf
  运行时对不上」——这是**运行期**才炸、也是 protobuf codegen 最难查的一类问题——
  在语法上无法发生。(Conan 为了近似这一点专门引入了 `<host_version>` 占位符;
  protobuf 自己的 CMake 至今有一个 open issue:CONFIG 模式下 `Protobuf_PROTOC_EXECUTABLE`
  被忽略。)
- **增量。** 生成是一条 ninja 边,`.proto` 变了才重跑,不变就不跑。
- **交叉编译构造上就对。** `--target` 下工具依然为构建机器构建,你什么都不用做。

> `grpc-plugin` 是独立的包而不是 `grpc` 里的一个 target,因为上游的插件只链
> `grpc_plugin_support` + protobuf,别的都不链 —— 一个代码生成器需要的是 `.proto`
> 解析器和 C++ 发射器,不是 TLS、DNS 和正则引擎。
> 详见 `.agents/docs/2026-08-05-codegen-ecosystem-design.md` §2。

## 平台支持

**linux 与 macOS。** 这不是 gRPC 的限制:`compat.openssl` 目前没有 windows 条目
(其描述符写着 "windows deferred —— 需要预编译的 MSVC 库"),因此在 windows 上依赖解析
会直接失败 —— `E_NOT_FOUND: package 'compat:openssl@3.5.1' not found`,还没开始编译。
gRPC 的 secure 构建无法去掉 TLS,所以本包的平台覆盖面就等于该依赖的覆盖面。
windows 所需的编译/链接选项已经写在 `mcpp.toml` 里,等那个条目落地即可启用。

## Features

| Feature | 默认 | 作用 |
|---|:---:|---|
| `ares` | **开** | c-ares 异步 DNS 解析器,与上游 gRPC 一致。用 `default-features = false` 关闭:7 个 TU 与 `compat.c-ares` 依赖一并移除,并定义 `GRPC_ARES=0`,gRPC 改用原生解析器 —— 即上游自己的 `grpc_no_ares=true` 配置。 |

```toml
[dependencies.grpc]
grpc = { version = "1.83.0", default-features = false }   # 不带 c-ares
```

## 构建方式

```
third_party/grpc-1.83.0/   钉住的上游源码,零补丁
src/grpc.cppm              C++23 模块接口(同时也是 lib root)
tools/gen_sources.py       从上游 CMakeLists.txt 重新生成源码清单
build.mcpp                 私有 include 目录 + `ares` 的关闭态
rules/                     `grpcgen` —— codegen 规则包(host module)
plugin/                    `grpc_cpp_plugin` —— 独立的 codegen 工具包
examples/greeter/          模板实例化:三行 build.mcpp
examples/helloworld/       同一个程序,规则手工摊开写
examples/advanced/         分层旋钮(extra_dirs / mock / plan-submit)
examples/shared-proto/     被 advanced 跨根生成的共享 .proto 树
```

`mcpp.toml` 里那份 995 条的源码清单是**上游自己的** —— `add_library(gpr)`、
`add_library(grpc)`、`add_library(grpc++)`、`add_library(address_sorting)` 四者之并 ——
CI 会跑 `tools/gen_sources.py --check`,证明 manifest 与 vendored 源码树没有漂移。

其中刻意排除了一个文件:`src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c`,
它与 `compat.protobuf` 的 `upb` feature 已经编译的 bootstrap 版本**逐字节完全相同**,
两份同时编入会重复符号。

### 为什么是独立仓库而不是索引描述符

gRPC **不发布任何自包含的源码产物**。它的 tag 归档里 abseil、protobuf、re2、boringssl、zlib
全是*空的 submodule 占位*,索引描述符的 `url` + `sha256` 无处可指。本仓库的 release tarball
就是那个产物。

### 依赖

| 包 | 原因 |
|---|---|
| `compat.abseil` | gRPC 的基础库,也是 protobuf 的 |
| `compat.protobuf` + `upb` | C++ 运行时,外加 gRPC 生成的 `upb-gen` 代码所链接的 C 运行时 |
| `compat.re2` | xds 路由匹配器 |
| `compat.c-ares` | 异步 DNS(`ares` feature) |
| `compat.openssl` | TLS。gRPC 官方支持 OpenSSL —— `src/core` 里只有 9 个文件带 BoringSSL 分支,且 OpenSSL 是默认那一侧 |
| `compat.zlib` | gRPC 自己的消息压缩(`deflate`/`inflate`) |

## 本地构建与测试

```bash
mcpp test                      # 构建库并运行模块测试
cd examples/greeter && mcpp run      # 三行 build.mcpp(经 grpcgen)
cd examples/helloworld && mcpp run   # 同一个程序,规则手写
```

## License

模块层与构建胶水为 Apache-2.0,与 vendored 的 gRPC 一致([LICENSE](LICENSE))。
各依赖保留其自身许可证。
