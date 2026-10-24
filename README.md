# grpc-m

gRPC **1.83.0** as a C++23 module, keeping `grpc::` APIs and protobuf code generation together.

[简体中文](README.zh.md)

## Quick start

Use mcpp **2026.10.24.1**. This candidate requires index publication.

```sh
mcpp new greeter --template grpc.grpc@1.83.0.1
cd greeter
mcpp run
```

The default template generates its stubs, starts a loopback server and checks real RPC replies.

## Existing projects

```toml
[dependencies]
"grpc.grpc" = { version = "1.83.0.1", features = ["codegen"] }
```

Put your schema in `proto/helloworld.proto` and add `build.mcpp`:

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

Put textual includes before the import. The template supplies the codegen build program.
`codegen` provides matching protoc, the C++ plugin and the rule; add your schemas under `proto/`.

## Scope

Linux and macOS are supported by this package's dependency set; Windows remains deferred.
The default `ares` feature uses c-ares. `default-features = false` selects the native resolver.
Host tools are built for the build machine, including when cross-compiling the application.

## Documentation

[Usage and codegen](docs/usage.md) · [Greeter](examples/greeter) ·
[Advanced codegen](examples/advanced) · [Apache-2.0](LICENSE)
