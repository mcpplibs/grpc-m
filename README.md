# grpc-m

> gRPC 1.83.0 from source, preserving grpc:: and sharing indexed dependencies.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/grpc/grpc) · [Releases](https://github.com/mcpplibs/grpc-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache-2.0-blue.svg)](LICENSE)
[![CI](https://github.com/mcpplibs/grpc-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/grpc-m/actions/workflows/ci.yml)

## Modules

- `import mcpplibs.mwrap.grpc;`: grpc:: client/server and status APIs
- `import mcpplibs.mwrap.grpc.macros;`: Version macro mappings

> More Modules API -> [Usage and API](docs/usage.md)

## Quick Start

```sh
mcpp new greeter --template mcpplibs.mwrap.grpc@1.83.0
cd greeter
mcpp run
```

```toml
[dependencies]
mcpplibs.mwrap.grpc = { version = "1.83.0", features = ["codegen"] }
```

## Two ways to use it

```cpp
#include "helloworld.grpc.pb.h"
import mcpplibs.mwrap.grpc;
```

Include standard/generated headers before imports. Plain `<grpcpp/grpcpp.h>` also works.
`.macros` maps version constants; preprocessing macros remain textual.

## Code generation

`codegen` supplies protoc, grpc_cpp_plugin and the host rule; the default template generates its stubs.
[Rule API and generated-header ordering](docs/usage.md)

## When you need more control

Use `grpcgen::generate_all`, `plan_all` and `submit` for import paths, plugins and explicit build edges.

## Platform support

Runtime: Linux/macOS. Windows runtime is unavailable through the current OpenSSL provider; host tools are tested separately.

## Features

| Feature | Purpose |
|---|---|
| `codegen` | protoc, C++ plugin and host rule |
| `ares` | c-ares DNS resolver |

## How it is built

Pinned gRPC sources plus indexed dependencies; private configuration stays in `build.mcpp`. [Details](docs/usage.md)

## Build and test locally

```sh
mcpp build
mcpp test
```

## License

[Apache-2.0](LICENSE); preserve third-party dependency notices.
