// gRPC "hello world", end to end in ONE process.
//
// Upstream's example is two binaries (greeter_server / greeter_client) you run
// by hand. That shape cannot assert anything, so this collapses it into a
// single program that stands a real server on a real loopback port, dials it
// over a real HTTP/2 channel, makes a real unary RPC, and checks the answer —
// exercising the same stack the two-binary version does:
//
//   ServerBuilder / Server        -> src/cpp/server/**
//   Greeter::Service (generated)  -> gen/helloworld.grpc.pb.cc
//   HelloRequest / HelloReply     -> gen/helloworld.pb.cc  (protobuf runtime)
//   CreateChannel / Stub          -> src/cpp/client/**
//   the wire itself               -> src/core/** (HTTP/2, transport, iomgr)
//
// Returns non-zero on any mismatch, so `mcpp run` doubles as the test.
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "helloworld.grpc.pb.h"

namespace {

class GreeterService final : public helloworld::Greeter::Service {
    grpc::Status SayHello(grpc::ServerContext* /*context*/,
                          const helloworld::HelloRequest* request,
                          helloworld::HelloReply* reply) override {
        if (request->name().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "name must not be empty");
        }
        reply->set_message("Hello " + request->name());
        return grpc::Status::OK;
    }
};

}  // namespace

int main() {
    GreeterService service;

    // Port 0 => the OS picks a free one and hands it back, so this never
    // collides with whatever else is running on a CI machine.
    int port = 0;
    grpc::ServerBuilder builder;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server || port == 0) {
        std::puts("FAIL: server did not start");
        return 1;
    }
    const std::string target = "127.0.0.1:" + std::to_string(port);
    std::printf("server listening on %s\n", target.c_str());

    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = helloworld::Greeter::NewStub(channel);

    int rc = 0;

    // 1. The happy path.
    {
        helloworld::HelloRequest request;
        request.set_name("mcpp");
        helloworld::HelloReply reply;
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));

        const grpc::Status status = stub->SayHello(&ctx, request, &reply);
        if (!status.ok()) {
            std::printf("FAIL: SayHello: %d %s\n", static_cast<int>(status.error_code()),
                        status.error_message().c_str());
            rc = 1;
        } else if (reply.message() != "Hello mcpp") {
            std::printf("FAIL: got \"%s\", want \"Hello mcpp\"\n", reply.message().c_str());
            rc = 1;
        } else {
            std::printf("Greeter replied: %s\n", reply.message().c_str());
        }
    }

    // 2. The error path must ALSO cross the wire: an empty name has to come
    //    back as INVALID_ARGUMENT from the server, not as a local success.
    //    Without this a stub that answered everything OK would pass.
    if (rc == 0) {
        helloworld::HelloRequest request;  // name left empty on purpose
        helloworld::HelloReply reply;
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));

        const grpc::Status status = stub->SayHello(&ctx, request, &reply);
        if (status.error_code() != grpc::StatusCode::INVALID_ARGUMENT) {
            std::printf("FAIL: empty name gave %d, want INVALID_ARGUMENT\n",
                        static_cast<int>(status.error_code()));
            rc = 1;
        } else {
            std::printf("Greeter rejected the empty name: %s\n", status.error_message().c_str());
        }
    }

    server->Shutdown();
    server->Wait();

    std::puts(rc == 0 ? "helloworld: OK" : "helloworld: FAILED");
    return rc;
}
