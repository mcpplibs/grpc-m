import mcpplibs.mwrap.grpc;
namespace macros = mcpplibs::mwrap::grpc::macros;
int main() { static_assert(macros::GRPC_CPP_VERSION_MAJOR == 1); static_assert(macros::GRPC_CPP_VERSION_MINOR == 83); return macros::GRPC_CPP_VERSION_STRING[0] != '1'; }
