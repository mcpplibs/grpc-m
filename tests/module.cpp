// Proves the `grpc` module is USABLE, not merely compilable.
//
// Everything here goes through `import grpc;` — there is no <grpcpp/...>
// include anywhere in this file — and every call below reaches a real symbol
// in libgrpc.a. A module that exported names but linked to nothing would fail.
//
// A full server + client round trip is NOT done here: gRPC rejects a server
// with neither a registered service nor a polled completion queue ("At least
// one of the completion queues must be frequently polled"), and registering a
// service needs protoc output, which belongs in the example rather than in a
// test of the module surface. examples/helloworld covers that path end to end.
//
// Note the ORDER: every textual #include comes first, `import grpc;` last.
// That is not a style choice — this module carries the standard library in its
// BMI, so a std header included AFTER the import arrives a second time and
// GCC fails with "ambiguous overload for operator==" on std::string. See the
// comment at the head of src/grpc.cppm.
#include <cstdio>
#include <memory>
#include <string>

import grpc;

int main() {
    // 1. A version string read from the library, not a header constant.
    const std::string version = grpc::Version();
    std::printf("grpc::Version() = %s\n", version.c_str());
    if (version.rfind("1.83", 0) != 0) {
        std::printf("FAIL: expected a 1.83.x runtime, got '%s'\n", version.c_str());
        return 1;
    }

    // 2. grpc::Status, including the error path — an all-OK stub cannot pass.
    const grpc::Status bad(grpc::StatusCode::INVALID_ARGUMENT, "nope");
    if (bad.ok() || bad.error_code() != grpc::StatusCode::INVALID_ARGUMENT ||
        bad.error_message() != "nope") {
        std::puts("FAIL: grpc::Status did not carry the error through");
        return 1;
    }
    if (!grpc::Status::OK.ok()) {
        std::puts("FAIL: grpc::Status::OK is not ok");
        return 1;
    }

    // 3. A real channel object over a real (unconnected) target. Channels
    //    connect lazily, so no server is needed; IDLE is the correct state
    //    before the first RPC, and getting it means the channel is a live
    //    object from the library rather than a null placeholder.
    auto channel = grpc::CreateChannel("127.0.0.1:1", grpc::InsecureChannelCredentials());
    if (!channel) {
        std::puts("FAIL: CreateChannel() returned null");
        return 1;
    }
    const auto state = channel->GetState(/*try_to_connect=*/false);
    if (state != GRPC_CHANNEL_IDLE) {
        std::printf("FAIL: fresh channel state is %d, want IDLE\n", static_cast<int>(state));
        return 1;
    }

    // 4. Channel arguments and a resource quota — plain library objects, but
    //    they prove those symbols link too.
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(4 * 1024 * 1024);
    args.SetString("grpc.primary_user_agent", "grpc-m-module-test");

    grpc::ResourceQuota quota("grpc-m-test");
    quota.Resize(1024 * 1024);

    // 5. Credentials factories, both sides.
    if (!grpc::InsecureChannelCredentials() || !grpc::InsecureServerCredentials()) {
        std::puts("FAIL: credentials factory returned null");
        return 1;
    }

    std::puts("module: OK");
    return 0;
}
