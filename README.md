# grpc-m

> gRPC 1.83.0 for mcpp — built from upstream source, `import grpc;` ready, one command to a working RPC

[![Release](https://img.shields.io/github/v/release/mcpplibs/grpc-m)](https://github.com/mcpplibs/grpc-m/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Module](https://img.shields.io/badge/module-ok-green.svg)](https://en.cppreference.com/w/cpp/language/modules)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)
[![gRPC 1.83.0](https://img.shields.io/badge/gRPC-1.83.0-brightgreen.svg)](https://github.com/grpc/grpc/releases/tag/v1.83.0)

| English - [简体中文](README.zh.md) |
|:---:|
| [mcpp build tool](https://github.com/mcpp-community/mcpp) · [package index](https://github.com/mcpplibs/mcpp-index) · [Issues](https://github.com/mcpplibs/grpc-m/issues) |

`grpc-m` builds gRPC's C++ stack from upstream source with **zero patches**, and takes
abseil, protobuf, re2, c-ares, OpenSSL and zlib from
[mcpp-index](https://github.com/mcpplibs/mcpp-index) rather than vendoring a second copy
of each. No CMake, no Bazel, no configure step — `mcpp build` is the whole story.

## Quick Start

```bash
mcpp new mygreeter --template grpc && cd mygreeter
mcpp run
```

```
server listening on 127.0.0.1:34733
Greeter replied: Hello mcpp
```

Or add it to an existing project:

```bash
mcpp add grpc.grpc
```

```toml
[dependencies.grpc]
grpc = "1.83.0"
```

## Two ways to use it

gRPC's code generator emits **headers**, so any real program includes protoc output. The
module is the ergonomic surface for everything you write by hand around it.

```cpp
#include <string>                     // std headers first
#include "helloworld.grpc.pb.h"       // then protoc output
import grpc;                          // the module LAST — see below

class Greeter final : public helloworld::Greeter::Service {
    grpc::Status SayHello(grpc::ServerContext*, const helloworld::HelloRequest* req,
                          helloworld::HelloReply* rep) override {
        rep->set_message("Hello " + req->name());
        return grpc::Status::OK;
    }
};
```

> **`import grpc;` goes last.** The module carries the standard library in its BMI (it
> wraps `<grpcpp/grpcpp.h>`, which pulls in most of it), so a textual `#include` *after*
> the import delivers a second copy and the build fails — `redefinition of
> std::__is_constant_evaluated`, or `ambiguous overload for operator==` on `std::string`.
> Put every `#include` above the import and everything resolves: the exported entities
> belong to the **global module**, so the textual and imported views are the same
> entities. Verified both ways round with gcc 16.1.0.

Prefer plain headers instead? That works too and needs no import at all:

```cpp
#include <grpcpp/grpcpp.h>
#include "helloworld.grpc.pb.h"
```

## Code generation

gRPC needs two host tools — `protoc` and `grpc_cpp_plugin` — and since **mcpp 2026.8.5.1**
you no longer supply them yourself. Declare them on the dependencies they belong to, add
the codegen rule, and that is the entire setup:

```toml
[dependencies.grpc]
grpc = { version = "1.83.0", features = ["codegen"] }
```

```cpp
// build.mcpp — in full
import mcpp;
import grpcgen;
int main() { return grpcgen::generate_all() ? 0 : 1; }
```

One dependency and one line. Which tools gRPC codegen needs — protoc, the C++
plugin, the rule that drives them — is this package's knowledge, and
`features = ["codegen"]` is where it says so. Adding a `.proto` means dropping
a file in `proto/`; there is no list to keep in sync.

That is `templates/greeter`, and `mcpp new --template grpc` gives it to you ready to
build (`--template` takes the PACKAGE; spell the template out as `grpc:greeter` when a
package ships more than one). **No generated file is checked in any more** — edit the `.proto` and rebuild.

`grpcgen` is an ordinary mcpp package holding the rule, written in C++ and versioned
alongside gRPC. Rules ship through the package manager you already have, so there is no
second language here the way xmake has Lua rules and Bazel has Starlark. It needs
**mcpp 2026.8.6.2** — 2026.8.5.2 is where a rule package first became able to use `import std;`
and `import mcpp;`.

Two examples, on purpose:

| | |
|---|---|
| `examples/greeter` | the template instantiated — 3-line `build.mcpp` via `grpcgen` |
| `examples/helloworld` | the same program with the rule written out by hand, so the mechanism stays legible |

Three properties this buys, none of which hand-managed codegen can offer:

- **A version mismatch is not expressible.** A tool's version *is* its dependency's
  version, so a `protoc` that disagrees with the protobuf runtime — a **runtime** failure,
  and the nastiest thing about protobuf codegen — cannot happen. (Conan needed a
  `<host_version>` placeholder to approximate this; protobuf's own CMake still has an open
  issue where `Protobuf_PROTOC_EXECUTABLE` is ignored in CONFIG mode.)
- **Incremental.** Generation is a ninja edge, so it re-runs when its `.proto` changes and
  not otherwise.
- **Cross-compilation is correct by construction.** Under `--target` the tools are still
  built for the build machine, with no action from you.

> `grpc-plugin` is a package of its own rather than a target inside `grpc`, because
> upstream's plugin links `grpc_plugin_support` + protobuf and nothing else — a code
> generator needs a `.proto` parser and a C++ emitter, not TLS, DNS and a regex engine.
> See `.agents/docs/2026-08-05-codegen-ecosystem-design.md` §2.

## Platform support

**linux and macOS.** Not a gRPC limitation: `compat.openssl` has no windows entry yet
("windows deferred — requires prebuilt MSVC libs"), so on windows dependency resolution
fails with `E_NOT_FOUND: package 'compat:openssl@3.5.1' not found` before anything is
compiled. gRPC's secure build cannot drop TLS, so this package's coverage is exactly
that dependency's. The windows compile/link flags are already in `mcpp.toml`, ready for
the day that entry lands.

## Features

| Feature | Default | Effect |
|---|:---:|---|
| `ares` | **on** | The c-ares asynchronous DNS resolver, matching upstream gRPC. Turn it off with `default-features = false`: 7 TUs and the `compat.c-ares` dependency drop out and `GRPC_ARES=0` is defined, so gRPC uses its native resolver — upstream's own `grpc_no_ares=true` configuration. |

```toml
[dependencies.grpc]
grpc = { version = "1.83.0", default-features = false }   # no c-ares
```

## How it is built

```
third_party/grpc-1.83.0/   pinned upstream source, zero patches
src/grpc.cppm              the C++23 module interface (also the lib root)
tools/gen_sources.py       regenerates the source list from upstream CMakeLists.txt
build.mcpp                 private include dirs + the `ares` off-state
rules/                     `grpcgen` — the codegen rule package (host module)
plugin/                    `grpc_cpp_plugin` — the codegen tool, its own package
examples/greeter/          the template instantiated: 3-line build.mcpp
examples/helloworld/       the same program with the rule written out by hand
```

The 995-entry source list in `mcpp.toml` is **upstream's own** — the union of
`add_library(gpr)`, `add_library(grpc)`, `add_library(grpc++)` and
`add_library(address_sorting)` — and `tools/gen_sources.py --check` runs in CI to prove
the manifest has not drifted from the vendored tree.

One file from that union is deliberately excluded:
`src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c`, which is byte-for-byte
identical to the bootstrap copy `compat.protobuf`'s `upb` feature already compiles —
building both is a duplicate-symbol failure.

### Why a repository rather than an index descriptor

gRPC publishes **no self-contained source artifact**. Its tag archive carries
abseil, protobuf, re2, boringssl and zlib as *empty submodule placeholders*, so there is
nothing an index descriptor could point a `url` + `sha256` at. This repository's release
tarball is that artifact.

### Dependencies

| Package | Why |
|---|---|
| `compat.abseil` | gRPC's base library, and protobuf's |
| `compat.protobuf` + `upb` | the C++ runtime, plus the C runtime gRPC's generated `upb-gen` code links against |
| `compat.re2` | the xds route matchers |
| `compat.c-ares` | asynchronous DNS (the `ares` feature) |
| `compat.openssl` | TLS. gRPC supports OpenSSL as a first-class provider — only 9 files in `src/core` have a BoringSSL branch, and OpenSSL is the default side |
| `compat.zlib` | gRPC's own message compression (`deflate`/`inflate`) |

## Build and test locally

```bash
mcpp test                      # builds the library, runs the module test
cd examples/greeter && mcpp run      # 3-line build.mcpp (via grpcgen)
cd examples/helloworld && mcpp run   # same program, rule written by hand
```

## License

The module layer and build glue are Apache-2.0, matching vendored gRPC
([LICENSE](LICENSE)). Each dependency keeps its own license.
