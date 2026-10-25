# Runnable template and native CI checkpoint

The default greeter template uses the upstream grpc namespace/API and generates a real service through the package-provided host plugin/rule. Both GCC 16 and LLVM 23 actual mcpp new consumers passed a loopback RPC plus the failure path. Template CI on source 839b61e passed; its Linux/macOS native workflow subsequently completed the advanced program but failed a stale generated-file assertion.

Current mcpp places generated files under an invocation/project directory. CI now checks common/types.pb.cc beside the actual orders_mock.grpc.pb.h instead of assuming target/.build-mcpp/out/common. It locates the Ninja graph by the generation edge rather than relying on filesystem order, which can select a host-tool graph. Local LLVM 23.1.3 mcpp run on examples/advanced passed in 3m40s; the produced nonempty shared-proto source, mock header and GENERATE edge descriptions were checked.

The pinned mcpp 2026.10.24.1 includes the object-layout cache fix for which the old MCPP_BUILD_CACHE=local workaround was added. Dropping that expired workaround lets sibling consumers reuse SDK objects; all examples still build and execute their generated code. Native source CI must revalidate the new checkpoint. Windows runtime remains unavailable with the declared OpenSSL provider; plugin/rule host-tool availability is tested separately through the index.

Root READMEs use the common concise style (52 lines per language); prior detailed guidance is under docs/. Package/runtime/plugin/rule versions stay coupled at 1.83.0.1. No merge, tag or release is included.
