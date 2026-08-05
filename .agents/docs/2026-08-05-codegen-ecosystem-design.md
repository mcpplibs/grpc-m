# grpc-m 全生态打通：让 codegen 不再签入仓库

> 状态：**已验证，实施中**
> 依赖：mcpp **2026.8.5.1**（#355 依赖产出的 host 工具、`mcpp:action=` 构建图节点、
> `host-module = true` 规则包）
> 涉及：本仓库的 `plugin/`（新）、`templates/`、`examples/`、`.github/workflows/ci.yml`；
> `mcpp-index` 的 `compat.protobuf` 与新条目 `mcpplibs.grpc-plugin`

---

## 0. 结论摘要

今天本仓库的 `mcpp.toml` 里写着：

```
# gen/ holds protoc output, CHECKED IN on purpose. gRPC's codegen needs two
# host tools — protoc and grpc_cpp_plugin — and mcpp has no way to hand a
# dependency's built binaries to a consumer.
```

那个「mcpp 没有办法」在 **2026.8.5.1** 之后不成立了。

**最重要的验证结果**：用 mcpp 从源码构建出的 `protoc` 与 `grpc_cpp_plugin`，对
`examples/helloworld/proto/helloworld.proto` 生成的**四个文件与仓库里签入的
逐字节相同**。也就是说，这条自建工具链的产物 ≡ 官方 protoc 35.1 + 官方 gRPC 1.83.0
插件的产物。

| 文件 | 结果 |
|---|---|
| `helloworld.pb.h` / `.pb.cc` | ✓ 逐字节相同 |
| `helloworld.grpc.pb.h` / `.grpc.pb.cc` | ✓ 逐字节相同 |

**三个此前不可能、现在结构性成立的性质**：

| 性质 | 今天（签入 gen/） | 本方案 |
|---|---|---|
| 改 `.proto` 后 | 手工重跑 protoc，忘了就一直用旧桩子 | ninja 边自动重跑，且只重跑受影响的 |
| protoc 与 protobuf 运行时版本 | **用户自己保证**，错配是**运行期**才炸 | **不可表达** —— 工具版本 ≡ 依赖版本 |
| 交叉编译 | 用户得自己找一个 host protoc | **构造上就对** —— 工具永远为 host 构建 |

---

## 1. gRPC codegen 需要什么

两个 host 二进制，且两者的版本都不自由：

| 工具 | 来自 | 版本约束 |
|---|---|---|
| `protoc` | `compat.protobuf` | 必须与链接的 protobuf **运行时**一致（35.1）—— 错配是运行期错误 |
| `grpc_cpp_plugin` | 本仓库（新 `plugin/` 包） | 必须与链接的 gRPC 一致（1.83.0）—— 生成代码调用 gRPC 内部 API |

「两个工具、两条版本约束、错配在运行期才炸」正是 `tools = [...]` 的**单一版本轴**要
解决的：工具的版本**就是**那条依赖的版本，所以错配**在语法上无法表达**。

对照业界：Conan 专门引入 `protobuf/<host_version>` 占位符来补这一点；protobuf 自己的
CMake 至今有一个 open issue（#14576）是 CONFIG 模式下 `Protobuf_PROTOC_EXECUTABLE`
被忽略；xmake 的 protobuf 包在交叉编译时**直接删掉** protoc 并且不接进 PATH。
mcpp 这里不需要额外机制，因为版本轴本来就只有一条。

## 2. 插件必须是**独立的包**，不能是 grpc-m 的一个 target

这是本方案里唯一一个「先猜错、被数据纠正」的决定，值得完整记下来。

**初版做法**：在 grpc-m 主包里加 `[features.codegen]` + `[targets.grpc_cpp_plugin]`，
用 `forward = ["compat.protobuf/protoc"]` 把成本门跨包传过去。语法上全部成立。

**为什么不行**：mcpp 把一个包编成**一个对象池**，`Target` 没有 `sources` 字段，
所以一个 `kind="bin"` 目标会链接**该包的全部对象**。而 upstream 的插件只链
`grpc_plugin_support` + protobuf，**完全不碰 gRPC 运行时**：

```
add_executable(grpc_cpp_plugin  src/compiler/cpp_plugin.cc)
target_link_libraries(grpc_cpp_plugin  grpc_plugin_support)
```

放进主包意味着这个代码生成器要链接 gRPC 的 ~1000 个 TU，并**继承 grpc-m 的整套依赖**：
OpenSSL、re2、c-ares、zlib。实测直接失败：

```
error: xlings install_packages failed for 'compat.openssl@3.5.1'
```

一个代码生成器因为**装不上 TLS 库**而构建失败 —— 这不只是「大和慢」，是依赖图本身错了。

**结论**：拆成独立的 `plugin/` 包，依赖只有 `compat.protobuf`。这恢复了 upstream 的
真实依赖图：3 个 TU + libprotoc，没有 TLS、没有 DNS、没有正则引擎。实测构建 **2.19s**
（protobuf 走全局缓存）。

## 3. 插件包只取 C++ 的那条闭包

upstream 的 `grpc_plugin_support`（CMakeLists.txt:6474）带全部 8 个语言 generator，
因为它同时支撑 `grpc_php_plugin` / `grpc_python_plugin` 等。本包只建 `grpc_cpp_plugin`，
所以只取 `cpp_plugin.cc` 实际够得到的：

```
src/compiler/cpp_generator.cc        C++ 发射器
src/compiler/proto_parser_helper.cc  cpp_generator 用它取注释/前导细节
```

**这不是抄近路，而是正确的闭包，并且它 MATTERS**：php 与 objective-c 的 generator 引用
libprotoc 内部符号（`compiler::objectivec::FileClassPrefix`、php 的若干 helper），
而 compat.protobuf 编译的源码集并不导出它们 —— 把它们带上会在**链接期**因为「没人要求
生成的语言」而失败。实测确认。

> 本仓库既有的纪律是「vendored 源码列表是 upstream 自己的，转录而来」
> （`tools/gen_sources.py --check` 为此存在）。那条纪律说的是**库**的源码列表；插件包
> 取的是「`grpc_cpp_plugin` 这一个可执行文件的闭包」，与 upstream 的
> `add_executable(grpc_cpp_plugin …)` 一致。

### 3.1 三个 vendored 头

插件源码只引用两个 gRPC 公开头：`grpcpp/impl/codegen/config_protobuf.h` 与
`grpcpp/ports_undef.inc`（连带 `ports_def.inc`）。三个都**自包含**
（`config_protobuf.h` 只 include protobuf 的头）。

它们 vendor 在 `plugin/include/` **而不是**去主包的 `third_party/…` 里取：这个包会作为
**自己的 tarball** 发布，`../third_party/…` 的 include 在本地能解析，一旦从索引消费就断。

## 4. 索引侧（mcpp-index）

### 4.1 `compat.protobuf` 加 protoc 目标

```lua
targets = {
    ["protobuf"] = { kind = "lib" },
    ["protoc"]   = { kind = "bin",
                     main = "*/src/google/protobuf/compiler/main.cc",
                     required_features = { "protoc", "upb" } },
},
features = {
    ["protoc"] = { sources = { … 138 项 libprotoc_srcs … } },
}
```

三个实测要点：

1. **138 项来自 upstream 自己的 `src/file_lists.cmake` 的 `libprotoc_srcs`**，不是手挑；
   与 libprotobuf 的源码集**零重叠**（`importer.cc`/`parser.cc` 早在 libprotobuf 里）。
   源码树里**没有** `.h.in` / `.cmake.in`，不需要任何 configure 步骤。
2. **必须同时要求 `upb`**：只开 `protoc` 会在链接期缺一批 `upb_*` 符号 —— libprotoc 的
   upb 生成器需要 upb 运行时。
3. **`main` 要写成 `*/src/...`**：Form B 包的源码在版本目录下的包装目录里，`*` 代表
   tarball 顶层文件夹名。mcpp 2026.8.5.1 起 `main` 会像 `sources` 一样展开这个 glob。

### 4.2 新条目 `mcpplibs.grpc-plugin`

与 `mcpplibs.grpc` 同一个仓库、同一个 tag，Form A（自带 `plugin/mcpp.toml`）。
平台覆盖是 **linux/macos/windows** —— 它只需要 libprotoc，**不受 compat.openssl 的
windows 缺口限制**（主包受）。

## 5. 用户侧：怎样才算「最方便」

### 5.1 本方案落地后

```toml
[dependencies]
grpc            = "1.83.0"
grpc-plugin     = { version = "1.83.0", tools = ["grpc_cpp_plugin"] }
compat.protobuf = { version = "35.1",   tools = ["protoc"] }
```

模板直接给出可用的 `build.mcpp`，用户 `mcpp new --template greeter` 之后改 `.proto`
即可，**不需要理解 action 的细节**。

### 5.2 下一步：规则包（不在本次范围，设计在此）

| | 用户要写 |
|---|---|
| CMake + vcpkg/Conan | `protobuf_generate(TARGET app)` ≈ 1 行（交叉时要自己处理 host protoc） |
| xmake | `add_rules("protobuf.cpp")` ≈ 1 行（交叉下 protoc **没接通**） |
| **mcpp 本方案** | 约 20 行 build.mcpp |
| mcpp + 规则包 | **约 3 行** |

`host-module = true` 就是为这一步做的：规则以**普通 mcpp 包**分发，消费者
`import mcpp.rules.grpc;`。规则因此有版本、能测试、能发布，而且是 **C++** 写的 ——
不引入第二门语言（xmake 用 Lua rule、Bazel 用 Starlark）。

**为什么不在本次做**：规则包要有自己的发布节奏；而且有一个**已知的引擎缺口**要先补 ——
一个 `host-module` 规则包**带不动自己的 tools**：工具的环境变量按「请求它的那个包」
记账，而规则代码是在**消费者**的 build.mcpp 里执行的，于是消费者看不到那个变量。
补法是让 host-module 依赖的 tools 转发给消费者，是一处小改动，但要单独做。

## 6. 已知缺口（都不阻塞本方案）

1. **工具子构建不继承 root 的 `[indices]`**。子构建以工具包为 root 重新解析依赖，
   用的是工具包自己的 manifest；消费者写在自己 manifest 里的 `[indices]` 覆盖不过去。
   对**已发布**的索引没有影响，但用本地索引做验证时会踩：现象是工具包解析到了**线上**
   的依赖版本而不是你改过的那份。
2. **插件包与主包同 tag 但是两个索引条目**，版本必须一起 bump。CI 应当校验二者相等 ——
   否则用一个 1.83.0 的 gRPC 配一个别的版本的插件，正是本方案要消灭的那类错配。

## 7. 验证矩阵（✓ = 已实测）

| 项 | 结果 |
|---|---|
| ✓ protoc 可从 compat.protobuf 源码构建 | 138 TU，无 configure 步骤 |
| ✓ grpc_cpp_plugin 可构建 | 3 TU + libprotoc，**2.19s**（protobuf 命中全局缓存） |
| ✓ 两者生成的桩子正确 | 四个文件与仓库签入的**逐字节相同** |
| ✓ 工具进全局 store 并跨工程复用 | 二次构建不重建 |
| 生成的桩子能编能跑 | example 去签入 gen/ 后 `mcpp run` 完成真实 RPC |
| 改 `.proto` 触发重新生成 | 且不相关的重建不触发 |

## 8. 明确不做

- **不裁剪主包的库源码列表**：那条 `gen_sources.py --check` 的保证不动。
- **不在本次发规则包**：先补 §5.2 的引擎缺口。
- **不动主包的平台覆盖**：它等于 `compat.openssl` 的，与 codegen 无关；插件包不受此限。
