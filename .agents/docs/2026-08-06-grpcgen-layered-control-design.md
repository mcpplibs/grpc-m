# grpcgen 的分层控制:让「方便」与「可控」不是两条路

> 状态：**设计待 review**
> 涉及：`rules/src/grpcgen.cppm`、`templates/greeter/`、`mcpp.toml` 的 `[feature-deps.codegen]`
> 前置：mcpp 2026.8.6.2（`reexport` / `rerun_if_changed_glob`）已发布

---

## 0. 现状与问题

`generate_all()` 一行就能跑通,底子是对的:工作被**声明**成构建图的边(`mcpp::action`)而不是当场执行,所以增量、并行、失败归属到具体那条边;错误信息直接告诉用户该往 mcpp.toml 里加哪一行。

问题不在「方便」,在**可控的层次只有两级**:

```cpp
struct options {
    std::string_view proto_dir = "proto";
    bool             grpc      = true;
};
```

两个旋钮之外是断崖。需求一超出,用户只能像 `examples/helloworld` 那样手写六十行 build.mcpp,**绕开整个规则**——而那六十行要重新实现规则里已经解决过的东西:well-known types 目录怎么定位、嵌套 `.proto` 的输出子目录怎么建、输入集合怎么算。它们会与规则悄悄漂移,且没有任何机制会报出漂移。

**断崖本身就是设计缺陷**:它把「优雅」和「可控」变成了二选一。

## 1. 原则:每一层是下一层的默认值,不是另一条路

只要下一层是「同一条代码路径 + 默认参数」,就不会出现「用了高级功能就失去便利」的分叉,也不会出现两套实现漂移。

这条原则决定了后面每一层的形状:L1 不是新函数,是 `options` 多几个字段;L2 不是新入口,是把 `bool grpc` 降级成语法糖;L3 不是旁路,是把现有函数拆成 `plan` + `submit` 两半,`generate_all()` 变成它俩的组合。

## 2. L0 — 默认(保持不变)

```cpp
import mcpp; import grpcgen;
int main() { return grpcgen::generate_all() ? 0 : 1; }
```

模板与文档的主线形态。本设计不改变它的任何行为。

## 3. L1 — 声明式旋钮

补上真实项目**必然**撞到的三个缺口。按撞到的频率排序:

| 缺口 | 什么时候遇到 | 证据 |
|---|---|---|
| 额外 import 路径 | 共享 proto 仓库;`google/api/annotations.proto`(gRPC-Gateway、Google API 风格接口) | 官方 `.proto` 之间互相 import 是常态,protoc 靠 `-I` 找 |
| mock 生成 | 一写单测就要 | `grpc_cpp_plugin` 自带 `generate_mock_code=true`,现在**完全没有办法开** |
| 任意 protoc 参数 | 兜住没想到的 | 例如 `--experimental_allow_proto3_optional` |

```cpp
grpcgen::generate_all({
    .imports     = {"../shared/proto"},
    .mock        = true,
    .protoc_args = {"--experimental_allow_proto3_optional"},
});
```

三条都是**加法**:不写等于今天的行为,逐字节相同。

`mock` 单独成字段而不是让用户自己往 `protoc_args` 塞,是因为它的拼写是插件参数(`--grpc_out=generate_mock_code=true:<dir>`)而不是 protoc 顶层参数——那正是「库该承担的知识」。

## 4. L2 — 插件列表:把 `bool grpc` 升成一等公民

```cpp
grpcgen::generate_all({
    .plugins = { grpcgen::cpp(), gateway_plugin, validate_plugin },
});
```

`grpc = true` 变成「列表里有 `cpp()`」的语法糖,旧写法一字不改。

**为什么必须做这一步**:`.proto` 的插件生态不止 gRPC——`protoc-gen-validate`、grpc-gateway、文档生成都是同一条 protoc 调用上的 `--<name>_out`。如果不做,每来一个插件就要往 `options` 上挂一个 `bool`,而那正是 `bool grpc` 已经示范过的坏形状。

一个插件的完整描述是:名字、可执行文件路径、输出目录、插件参数、以及它产出哪些文件后缀(决定 `output()` 声明)。后缀不能省——mcpp 需要知道产物才能把 `.cc` 纳入编译集、把 `.h` 排除在外。

## 5. L3 — `plan` / `submit` 分离:终极逃生舱

```cpp
auto edges = grpcgen::plan_all();            // 只构造边,不提交
for (auto& e : edges) e.arg("--whatever");   // 完全接管
grpcgen::submit(edges);
```

价值不在「能改 flag」——L1 的 `protoc_args` 已经覆盖大半——而在**再离谱的需求也不必绕开规则**:well-known types 定位、子目录创建、输入集合计算这些规则已经解决的部分继续复用,用户只接管自己关心的那一段。

这一层直接消灭第 0 节说的漂移风险。

## 6. 可观测性:分工已经由引擎定死

**核实过,不是推断**:`mcpp:action=` 声明的边,其完整命令行会原样落进 `build.ninja`:

```
rule mcpp_action_0
  command = .../bin/protoc -I.../proto -I.../protobuf/src --cpp_out=... --grpc_out=... 
  description = GENERATE protoc:echo
```

因此:

- **规则不该实现 `GRPCGEN_EXPLAIN` 之类的命令行 dump**。「这条边到底跑了什么」是引擎已经答完的问题,`ninja -t commands <output>` 即可取;规则重造一份只会有第二个真相来源。
- **规则该负责的是 `description`**。它现在是 `protoc:echo`,看不出开了哪些旋钮。改成自述:

```
protoc:echo (+grpc +mock, -I proto -I ../shared/proto)
```

这条串出现在每次构建的输出里,是**零成本**的可观测性:不需要任何额外命令,就能回答「这个 flag 到底进去没有」。而完整命令行仍在 build.ninja 里等着被查。

分工一句话:**引擎拥有「命令是什么」,规则拥有「哪些旋钮产生了它」。**

## 7. 命名:`codegen` 保留,真正要修的是文档

查证结论(不是印象):

- **`protoc` 是错的名字。** 它只命名了三者之一,而且是 **protobuf 那一半**;挂在 grpc 包上更偏。且 `buf` 存在(自带编译器,不需要 protoc 二进制),哪天换生成器,`protoc` 这个名字就成了谎话。
- **与 tonic 的歧义是单向的,撞不上。** tonic 的 `codegen` feature 指「生成的代码编译时需要的运行时导出」(`tonic::codegen` 模块),生成器那一半在 tonic 里是独立 crate `tonic-build`。C++ 里不存在对应物——生成的桩直接 `#include <grpcpp/...>` 并链同一个库。
- **L2 之后 `codegen` 更站得住**:它表达的是「这个包知道 `.proto` 怎么变成 C++,包括你后来加的插件」,而不是「跑哪个二进制」。`stubgen` 同样会被 L2 打脸(插件产出的不止 stub)。

**真正的缺口是文档没说清两件事:**

1. **它带来的是两步,不是一步。** 官方文档里是两条独立的 protoc 调用:

   ```
   protoc --cpp_out=.  route_guide.proto     → .pb.{h,cc}       protobuf 消息
   protoc --grpc_out=. --plugin=...          → .grpc.pb.{h,cc}  gRPC 服务桩
   ```

   第一步是 protobuf 的事,与 gRPC 无关;第二步才是。现在的注释只说「the whole toolchain」,读者意识不到 `.pb.*` 根本不属于 gRPC——而这正是「为什么需要 `compat.protobuf` 的 protoc」的答案。

2. **为什么默认关闭。** 现在只写了成本(protoc 拖进 libprotoc 约 157 个 TU),没写**真实存在的无 codegen 路径**:gRPC 官方的 Generic API(`grpc::GenericStub` + `grpc::ByteBuffer`,自己序列化)。官方性能文档明确推荐它用于高竞争或 proto 序列化 CPU 密集的场景——代理、负载均衡这类转发型服务走的就是这条路。也就是说 off-by-default 不是「省点编译时间」,是**存在一整类项目确实不需要它**。

## 7.5 实施中长出来的两条(设计里没有)

**`extra_dirs`。** 原设计只有 `.imports`(额外 `-I`)。写 `examples/advanced` 时撞出来:protoc 会把 `#include "common/types.pb.h"` 写进任何 import 了它的文件,所以**只靠 `-I` 够到的共享树会产出一个没人生成的头**,报错(`fatal error: 'common/types.pb.h' file not found`)离原因很远。两个概念因此必须分开:`extra_dirs` 既生成又搜索,`imports` 只搜索。

这条是示例发现的,不是设计发现的——一个只写文档不写示例的设计会把它漏掉。

**`plan_entries` / `entry`。** L3 的真正底层:`.proto` 按 `(root, name)` 寻址。`plan()` 与 `plan_all()` 都汇入它。它必须是公开的,因为那是「`.proto` 既不在 `proto_dir` 也不在 `extra_dirs` 下」这种项目的唯一出路——否则它们又回到自己写 build.mcpp,而那正是本设计要消掉的断崖。

## 7.6 一个生态限制,如实记录

`.mock` 让 protoc 产出 `<stem>_mock.grpc.pb.h`,而那个头 `#include <gmock/gmock.h>` —— 本生态的 `compat.gtest` **只带 googletest、不带 gmock**。所以旋钮是对的、产物也真的生成,但**当前无法编译它**。

`examples/advanced` 因此声明并产出 mock 头、但不 include;plan 阶段断言它进了输出集,CI 再断言文件真的存在。补 gmock 是 mcpp-index 的事,与本规则无关。

## 8. 实施顺序

| 步 | 内容 | 风险 |
|---|---|---|
| 1 | L1 三个字段 + `description` 自述 | 低,纯加法,旧写法逐字节不变 |
| 2 | 文档:两步的分工 + Generic API 那条路 | 无 |
| 3 | L2 插件列表(`grpc = true` 降级为语法糖) | 中——新的用户可见 API,要先定 plugin 的完整描述形状 |
| 4 | L3 `plan` / `submit` | 中——`generate_all` 必须变成二者的组合,否则又是两条路 |

建议 1+2 先落地并发布,验证过再做 3+4。1+2 覆盖真实项目 90% 的需求,且不引入任何新概念。

## 9. 验证

- **单测无从下手**(规则运行在 build.mcpp 里),所以验证靠 examples:
  - `examples/greeter` 保持 L0 不变 —— 证明加法没有改变默认行为;
  - 新增一个用到 `.imports` + `.mock` 的示例,断言 mock 头文件真的产出、且能被 include;
  - `description` 自述:断言构建输出里出现 `+mock`。
- **不做的验证**:不再手工比对 protoc 命令行——它在 build.ninja 里,是引擎的契约,不是本规则的。

## 10. 明确不做

- **不实现命令行 dump**(`GRPCGEN_EXPLAIN`)。引擎已经把完整命令写进 build.ninja,第二个真相来源只会漂移。
- **不改 feature 名。** 见 §7。
- **不为每个插件加一个 `bool`。** 那是 L2 要消灭的形状,而不是要复制的。
- **不支持「跳过 protobuf 那一步只生成 gRPC 桩」。** protoc 的 `--grpc_out` 产出的桩 `#include` 对应的 `.pb.h`,两步在 C++ 里不可分。
