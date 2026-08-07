# 在 C++ 里用 gRPC:开发体验对比

> 日期:2026-08-07
> 对象:CMake(上游官方路径)· CMake + vcpkg/Conan · Bazel · xmake · mcpp
> 范围:**只谈开发体验**——从零跑通、日常循环、出错那一刻、需求超出默认那一刻、换机器。
> 不谈:性能、包覆盖面的绝对数量、许可与治理。

**利益披露**:mcpp 是本文作者参与的项目。因此每个场景都先写对手方的真实优势,
mcpp 的代价单列一节(§9),且全文区分证据等级:

- `[实测]` —— 本次在 xlings subos 沙箱中亲自跑出的结果(Linux x86_64,CN 镜像,
  全新 MCPP_HOME,mcpp 2026.8.6.3,grpc-m v1.83.0-4)。
- `[引用]` —— 上游仓库文件的实际内容,链接见文末。
- 无标注 —— 形态性描述,可从公开文档直接查证。

---

## 1. 结论先行

C++ 的 gRPC 开发体验,痛点从来不是「链接一个库」,而是三件事:

1. **`protoc` 与 `grpc_cpp_plugin` 必须和你链接的运行时同源**——错开时报错出现在
   生成代码深处,离原因很远;
2. **桩不是手写代码,但要参与构建图**——否则增量、并行、失败归因全都没有;
3. **需求一超出默认,构建文件就开始膨胀**——而膨胀出来的那部分会与工具悄悄漂移。

成熟工具在第 2 点上已经拉平(都做成了构建图节点)。真正拉开差距的是第 1 点和第 3 点,
以及一个很少被讨论但每天都在消耗人的维度:**你需要预先知道多少事,才能写对第一行**。

---

## 2. 对比矩阵

符号是**相对排序**,不是绝对评价:`◎` 该维度上的强项 · `○` 可用、无明显摩擦 ·
`△` 可行但有条件或需额外工作 · `✕` 明确弱项。带 `[实测]`/`[引用]` 的单元格有硬证据,
其余是形态判断。

### 2.1 开发体验矩阵

| 方面 | CMake(官方) | CMake+vcpkg/Conan | Bazel | xmake | mcpp |
|---|:--:|:--:|:--:|:--:|:--:|
| **上手门槛**<br><sub>跑通首个 RPC 前要理解的概念</sub> | ✕<br><sub>三条分支的存在</sub> | △<br><sub>+ 包管理器一套</sub> | ✕<br><sub>整套 Bazel 世界观</sub> | ◎<br><sub>rule + 包</sub> | ◎<br><sub>一条依赖</sub> |
| **声明成本**<br><sub>最小可用形态</sub> | ✕<br><sub>110 行 + 14/proto `[引用]`</sub> | ✕<br><sub>同左</sub> | ○<br><sub>13 行/proto `[引用]`</sub> | ◎<br><sub>2–3 行</sub> | ◎<br><sub>1 依赖 + 1 行 `[实测]`</sub> |
| **演进摩擦**<br><sub>新增一个 `.proto`</sub> | △<br><sub>改构建文件</sub> | △ | △<br><sub>改 BUILD(可 glob)</sub> | ◎<br><sub>通配符覆盖即免改</sub> | ◎<br><sub>免改 `[实测]`</sub> |
| **增量与并行**<br><sub>日常循环</sub> | ○ | ○ | ◎<br><sub>+ 远程缓存</sub> | ○ | ○<br><sub>`[实测]` 11.5s</sub> |
| **版本同源**<br><sub>生成器 ↔ 运行时</sub> | △<br><sub>交叉时退回宿主 `[引用]`</sub> | ◎<br><sub>lockfile 同锁</sub> | ◎<br><sub>同棵源码树</sub> | ○<br><sub>同一包提供</sub> | ◎<br><sub>结构性:同包 host 子构建</sub> |
| **交叉编译 host 工具** | ✕<br><sub>需自行处理</sub> | △ | ◎<br><sub>exec/target 一等</sub> | △ | ◎ |
| **定制阶梯**<br><sub>超出默认时</sub> | ◎<br><sub>无起点也无天花板</sub> | ◎ | ◎<br><sub>Starlark 任意规则</sub> | △<br><sub>需自写 Lua rule</sub> | ◎<br><sub>L0→L3 无断崖</sub> |
| **旋钮可观测性**<br><sub>「我加的 flag 进去没有」</sub> | ○<br><sub>查生成的构建文件</sub> | ○ | ◎<br><sub>`aquery` 查动作图</sub> | △<br><sub>verbose 输出</sub> | ◎<br><sub>description 自述 + build.ninja</sub> |
| **环境复现 / 新人入职** | ✕<br><sub>宿主装什么算什么</sub> | ○<br><sub>库可复现,工具链不管</sub> | ○<br><sub>hermetic 需专门配置</sub> | ○ | ◎<br><sub>只要 mcpp `[实测]`</sub> |
| **IDE 集成**<br><sub>`compile_commands.json`</sub> | ○<br><sub>一个开关</sub> | ○ | △<br><sub>需第三方抽取器</sub> | ○<br><sub>一条命令</sub> | ◎<br><sub>默认产出且含生成桩 `[实测]`</sub> |
| **心智负担**<br><sub>要学的第二门语言</sub> | △<br><sub>CMake DSL</sub> | ✕<br><sub>DSL + 包管理器</sub> | ✕<br><sub>Starlark</sub> | △<br><sub>Lua</sub> | ◎<br><sub>规则 API 是普通 C++</sub> |
| **生态覆盖面** | ◎ | ◎<br><sub>包最全</sub> | ○ | ○ | ✕<br><sub>最小,见 §9</sub> |

### 2.2 可核验数字矩阵

| 量 | CMake(官方) | CMake+vcpkg/Conan | Bazel | xmake | mcpp |
|---|---|---|---|---|---|
| 共享样板行数 | ~110(三分支)`[引用]` | ~110 | 0(规则内置) | 0 | 0 |
| 每个 `.proto` 行数 | ~14 `[引用]` | ~14 | 13 / 3 条规则 `[引用]` | 1 行 `add_files` | 0 |
| 新增 `.proto` 要动的文件 | 1(CMakeLists) | 1 | 1(BUILD) | 0(若通配符覆盖) | 0 `[实测]` |
| 新机器需预装 | 编译器 + CMake + 库 | 编译器 + CMake + 包管理器 | Bazel(+ 工具链配置) | xmake + 编译器 | 仅 mcpp `[实测]` |
| 冷启动到跑通 | 取决于是否源码构建 gRPC | 二进制包,较快 | 首次拉源码较久 | 二进制包,较快 | 216s `[实测]` |
| 增量(改一行) | 秒级 | 秒级 | 秒级(+远程缓存) | 秒级 | 11.5s `[实测]` |
| gRPC 平台覆盖 | 三平台 | 三平台 | 三平台 | 三平台 | Linux/macOS |

> 两张表要一起读:2.1 里 mcpp 的 `◎` 集中在**摩擦**类维度,`✕` 在生态;
> 2.2 的数字说明这些 `◎` 不是主观感受。而 CMake 系的 `✕` 集中在**起点**,
> 它的 `◎` 在**上限**——这正是 §7 要展开的那条:曲线形状不同,不是高低不同。

---

## 3. 场景一:从零到第一个 RPC 跑起来

### CMake(上游官方路径)

上游 `examples/cpp/cmake/common.cmake` 约 **110 行,三条分支**(submodule /
FetchContent / 预装),分别求出 `_PROTOBUF_PROTOC` 与
`_GRPC_CPP_PLUGIN_EXECUTABLE`。`[引用]`

开发体验上的实际负担不是「110 行要抄」,而是**你必须先理解这三条分支的存在**,
才能判断自己属于哪一条、以及为什么交叉编译时行为会变。第一次上手的人通常是
复制粘贴 + 遇错再查,而错误信息(`find_package(gRPC) not found`、
插件版本不匹配)并不会告诉你分支选错了。

此外 gRPC 本体要么源码构建(耗时可观),要么依赖系统里已有的安装。

### CMake + vcpkg / Conan

**这是当前最省心的成熟组合。** 包管理器解决了两件对 DX 影响最大的事:
gRPC 与 protobuf 由预编译二进制提供(不必等源码构建),且 lockfile
把两者版本一起锁住——§6 的同源问题在这条路上基本消失。

代价:codegen 样板一行没少,`common.cmake` 那部分照写。

### Bazel

`bazel build //...` 之前要先装 Bazel、写 WORKSPACE/bzlmod。好处是 gRPC
本身就用 Bazel 构建,支持是一等的;声明也整齐:

```python
proto_library(name = "helloworld_proto", srcs = ["helloworld.proto"])
cc_proto_library(name = "helloworld_cc_proto", deps = [":helloworld_proto"])
cc_grpc_library(name = "helloworld_cc_grpc", srcs = [":helloworld_proto"],
                grpc_only = True, deps = [":helloworld_cc_proto"])
```

每个 proto **3 条规则 / 13 行**。`[引用]` 概念清楚,但它要求你先接受
Bazel 的整套世界观(workspace、label、target 命名),这对只想跑个 RPC 的人是笔大投入。

### xmake

内置 rule,写法很短:

```lua
add_requires("grpc")
add_rules("protobuf.cpp")
add_files("src/*.proto", {proto_rootdir = "src", proto_grpc_cpp_plugin = true})
```

这是成熟工具里 DX 最好的一档:两三行,通配符,包管理内置。

### mcpp

```toml
[dependencies.grpc]
grpc = { version = "1.83.0", features = ["codegen"] }
```

```cpp
// build.mcpp
import mcpp;
import grpcgen;
int main() { return grpcgen::generate_all() ? 0 : 1; }
```

`[实测]` `mcpp new --template grpc:greeter` → `mcpp run`:

```
Finished dev [unoptimized + debuginfo] in 216.24s
server listening on 127.0.0.1:46009
Greeter replied: Hello mcpp
helloworld: OK
```

DX 上的差别不在行数,在**你需要预先知道的事**:这里没有「protoc 在哪」这个问题,
因为它不是用户要回答的问题。C++ 工具链(gcc/glibc/binutils)也由 mcpp 自己拉起,
机器上不需要预装任何东西。

**这一场景的排序(仅就上手负担)**:mcpp ≈ xmake > vcpkg/Conan+CMake > Bazel > 裸 CMake。

---

## 4. 场景二:改一行 `.proto` 的循环

这是开发者一天里重复最多次的动作,也是最该被优化的。

五种方案在这里**表现接近**:codegen 都是构建图节点,改 `.proto` 只重新生成受影响的桩,
并行执行,增量正确。Bazel 额外有远程缓存,在大型团队里是明显优势。

`[实测]` mcpp 侧的增量循环:

```
Cached grpc.grpc v1.83.0 (1003 units)
Finished dev [unoptimized + debuginfo] in 11.51s
```

**结论:这一维度不是差异点。** 它常被当作卖点,但其实是公共水位——
任何把 codegen 做成构建图节点的工具都能达到。真正的差别在下一节。

---

## 5. 场景三:加一个新的 `.proto`

| 工具 | 要动什么 |
|---|---|
| CMake(官方样板) | 改 CMakeLists:4 行变量 + ~10 行 `add_custom_command` `[引用]` |
| CMake + 包管理器 | 同上,包管理器不改变这部分 |
| Bazel | 改 BUILD:新增或扩展 `proto_library` 的 `srcs`(可用 `glob`) |
| xmake | 若已被 `add_files("src/*.proto", …)` 的通配符覆盖,**无需改动** |
| mcpp | **无需改动任何文件** |

`[实测]` 往 greeter 工程丢一个 `extra.proto`,**不碰 build.mcpp**,重新构建:

```
Finished dev in 11.63s
target/.build-mcpp/out/extra.pb.h
target/.build-mcpp/out/extra.grpc.pb.h   ← 生成并已编译进产物
```

这条 DX 差别比看上去重要:**「加文件要改构建文件」是一类持续的小摩擦**,
它让 `.proto` 目录与构建文件之间产生一份需要人来维护的重复清单,
而清单漂移时的症状(某个服务莫名没有桩)通常不会立刻被发现。

xmake 与 mcpp 通过通配符/glob 消掉了它;CMake 也可以自写 `GLOB CONFIGURE_DEPENDS`,
但 CMake 官方文档一直不推荐 glob,所以多数工程不会这么做。

---

## 6. 场景四:出错的那一刻

DX 的关键不是「会不会出错」,而是**错误离原因有多远**。

### 版本错开:最贵的一类

gRPC 的桩由 `grpc_cpp_plugin` 生成,而桩里 `#include` 的是你链接的那份
`libgrpc++` 的头。两者错开时,报错落在生成代码深处,提示词与真实原因毫无关联。

值得注意的是**上游官方样板自己就在交叉编译分支上放弃了这条保证** `[引用]`:

- 非交叉:`set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)` —— 与源码同源;
- 交叉:`find_program(_PROTOBUF_PROTOC protoc)` —— 宿主上碰巧装了哪个就用哪个。

这不是上游疏忽,而是 CMake 层面难以两全:交叉编译时你无法直接运行为目标平台构建的
protoc。Bazel 用 exec/target 平台分离一等地解决了它;vcpkg/Conan 用 lockfile 把
风险降到很低;mcpp 的做法是把工具当作**依赖的产出**——声明 `codegen` feature 时,
protoc 与 grpc_cpp_plugin 由「你正在链接的那个包」在 host 侧子构建出来,
**同源不是需要维护的约定,而是拿不到别的东西**。

### 构建期错误的归因

各家都把 codegen 做成图节点,失败时能指出是哪条命令,这一点上没有实质差别。
差别在**输出的可读性**:命令行越长,越难一眼看出「我加的那个 flag 到底进去没有」。

mcpp 侧的分工是:完整 argv 由引擎写进 `build.ninja`(`ninja -t commands <output>` 可取),
规则只负责在每次构建都打印的 description 里自述用了哪些旋钮:

```
GENERATE protoc:orders (+grpc +mock, -Iproto -I../shared-proto)
```

规则**不重造命令行 dump**——第二个真相来源只会漂移。

---

## 7. 场景五:需求超出默认的那一刻

这是我认为最值得比较、也最少被讨论的一维:**从「一行搞定」到「完全接管」之间,有没有断崖。**

### CMake / Bazel:没有起点,但也没有天花板

它们的 DX 曲线是平的:你从第一行就在写通用构建代码。想加第三方 protoc 插件、
想改任意参数,写法与原本没有区别——**因为原本就没有「简单模式」可失去**。
代价是起点本身很贵(§3)。

### 「一行搞定」类工具的典型陷阱

给了极简默认,却只配两三个旋钮:需求一超出,用户只能绕开规则手写全部逻辑,
而那些手写代码要重新实现规则已解决的问题(well-known types 定位、嵌套目录的输出
子目录、输入集合计算),并且会与规则悄悄漂移,没有任何机制报出漂移。

xmake 在这里的形态是:内置旋钮之外需要自写 Lua rule——**能力上没有上限**
(rule 系统很完整),但那是一次语境切换。

### mcpp 的四层

判据是**每一层都是下一层的默认值,而不是另一条代码路径**:

| 层 | 形态 | 恒等式 |
|---|---|---|
| L0 | `generate_all()` | — |
| L1 | 声明式旋钮:`extra_dirs` / `imports` / `mock` / `protoc_args` | `generate_all(opt)` |
| L2 | 插件列表:第三方插件与内置同级 | `.grpc = true` ≡ `.plugins = {cpp()}` |
| L3 | `plan` / `submit` 分离,拿到边任意改写 | `generate_all(opt) ≡ submit(plan_all(opt))` |

`[实测]` advanced 工程用的是 L1+L3 组合(跨根生成 + mock + 改写后提交),
依赖仍然只有一条:

```
service = orders.Orders
advanced: OK (messages + stubs, cross-root generation)
```

这里有一条**写文档写不出来、只有写示例才会撞到**的经验:protoc 会把
`#include "common/types.pb.h"` 写进任何 import 了它的文件,所以只靠 `-I`
够到的共享 proto 树会产出一个**没人生成的头**,报错离原因很远。因此
「额外搜索路径」与「额外生成根」必须是两个概念(`imports` vs `extra_dirs`)——
这类缺口是 DX 的真实成本,而它只在真正写一个跨根工程时才暴露。

### 另一种表达:规则 API 是普通 C++

`build.mcpp` 是一个用 `import grpcgen;` 的 C++ 程序,不是字符串 DSL。
拼错字段名是编译错误,IDE 有补全和类型。对比:CMake 是字符串 DSL,
Bazel 是 Starlark,xmake 是 Lua——都需要在构建配置期切换到另一门语言。
这一条是偏好问题,但对「已经在写 C++ 的人」是实打实的减负。

---

## 8. 场景六:换台机器 / 新同事入职

| 工具 | 新机器上要先装什么 |
|---|---|
| CMake(官方) | 编译器、CMake、(可能)系统 gRPC/protobuf;版本要对 |
| CMake + vcpkg/Conan | 编译器、CMake、包管理器;库由 lockfile 复现 |
| Bazel | Bazel;C++ 工具链默认取宿主,hermetic 需专门配置 |
| xmake | xmake;编译器多数情况取宿主 |
| mcpp | **只要 mcpp**;工具链与依赖全部自动拉起 |

`[实测]` 本次验证正是这个场景:全新 subos 沙箱 + 全新 MCPP_HOME,
除 mcpp 外未预装任何东西,`mcpp new` → `mcpp run` 直接跑通。

代价见 §9:首次要下载整套载荷(gcc 约 97 MB)。

---

## 9. mcpp 的开发体验代价

如实列出,这些都是选它要付的:

- **生态最小。** 索引里的包数量与 vcpkg/Conan 不在一个量级。遇到未收录的库,
  你要自己写描述符,而不是一条 `install` 命令。这是当前最大的 DX 差距。
- **要求 C++23 modules。** 工具链下限高,老编译器/老 SDK 的项目直接排除在外。
- **gRPC 目前仅 Linux / macOS。** 依赖链上的 openssl 尚无 Windows 条目。
- **mock 能生成不能编译。** `.mock` 产出的头 `#include <gmock/gmock.h>`,
  而生态里的 gtest 包目前不带 gmock——旋钮是对的,产物也真的生成,但当前编不过。
- **首次构建重。** 零系统依赖的另一面是要下载整套工具链载荷。`[实测]` gcc 载荷约 97 MB。
- **构建期噪声。** 当前每次构建会出现两条与本工程无关的 warning
  (模块扫描器把块注释里的 `import` 当成模块导入;非模块 lib 报缺 `.cppm` 根)。
  已开 issue:mcpp#373、mcpp#374。

---

## 10. 怎么选

- **已有 CMake 工程 / 需要最大库覆盖面** → CMake + vcpkg 或 Conan。
  样板一次写完不常动,包管理器把版本一致性补上了。这是最稳的默认答案。
- **多语言单仓 / 需要远程缓存与严格可复现** → Bazel。gRPC 自身就用它构建,
  支持最一等,代价是整套体系的学习与维护。
- **想要接近极简的写法、又需要成熟度与 Windows** → xmake。
  protobuf/gRPC 是内置 rule,是成熟工具里 DX 最好的一档。
- **新工程 + C++23 modules + Linux/macOS**,并且看重「一条依赖拿到全套工具链且版本
  必然同源」与「从一行到完全接管之间没有断崖」→ mcpp。
  目前更适合愿意参与生态建设的人,而不是要求开箱即用广度的团队。

---

## 11. 一句话结论

在**日常循环**上,几种成熟工具已经拉平;差别集中在**你需要预先知道多少事**、
**加一个文件要不要改构建文件**、以及**需求超出默认时是走上台阶还是掉下断崖**。

mcpp 试图改变的是其中两条:把「生成器与运行时同源」从约定变成结构,
把可定制性从断崖变成阶梯。是否值得,取决于你更怕「样板写一次」,
还是更怕「版本错开时那条离原因很远的报错」。

---

## 引用来源

- gRPC 上游 [`examples/cpp/cmake/common.cmake`](https://github.com/grpc/grpc/blob/master/examples/cpp/cmake/common.cmake) —— ~110 行,三条分支,交叉编译退回 `find_program`
- gRPC 上游 [`examples/cpp/helloworld/CMakeLists.txt`](https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/CMakeLists.txt) —— 全文 67 行,每 proto ~10 行 `add_custom_command`
- gRPC 上游 [`examples/protos/BUILD`](https://github.com/grpc/grpc/blob/master/examples/protos/BUILD) —— 每 proto 3 条规则 / 13 行
- protobuf [`cmake/protobuf-generate.cmake`](https://github.com/protocolbuffers/protobuf/blob/main/cmake/protobuf-generate.cmake) —— `PLUGIN` + `GENERATE_EXTENSIONS` 可产 gRPC 桩
- xmake [v2.8.1 发布说明](https://xmake.io/posts/xmake-update-v2.8.1) —— `protobuf.cpp` rule 支持 `proto_grpc_cpp_plugin`
- 本仓库 [`.agents/docs/2026-08-06-grpcgen-layered-control-design.md`](2026-08-06-grpcgen-layered-control-design.md) —— L0–L3 的设计判据
