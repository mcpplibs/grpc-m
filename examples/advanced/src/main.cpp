// Proves all three products of the L1 configuration exist and compile:
//   orders.pb.h        protobuf messages           (--cpp_out)
//   orders.grpc.pb.h   gRPC service stubs          (--grpc_out)
// The mock header (orders_mock.grpc.pb.h) is DECLARED by the edge and produced
// by protoc, but not included here: it pulls in <gmock/gmock.h>, which this
// ecosystem's compat.gtest does not ship. build.mcpp asserts it was planned.
//
// The cross-root import is proven by orders.pb.h alone: orders.proto imports
// common/types.proto, which protoc can only resolve through the extra -I.
#include <cstdio>
#include <string>

#include "orders.pb.h"
#include "orders.grpc.pb.h"

int main() {
    orders::PlaceRequest req;
    req.set_sku("mcpp-42");
    req.mutable_trace()->set_id("t-1");          // ← type from the SHARED root
    if (req.trace().id() != "t-1") return 1;

    const std::string svc = orders::Orders::service_full_name();
    std::printf("service = %s\n", svc.c_str());
    if (svc != "orders.Orders") return 1;

    std::printf("advanced: OK (messages + stubs, cross-root generation)\n");
    return 0;
}
