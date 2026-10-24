module;
#include <grpcpp/version_info.h>
namespace mwrap_grpc_capture {
inline constexpr auto value_GRPC_CPP_VERSION_MAJOR = GRPC_CPP_VERSION_MAJOR;
inline constexpr auto value_GRPC_CPP_VERSION_MINOR = GRPC_CPP_VERSION_MINOR;
inline constexpr auto value_GRPC_CPP_VERSION_PATCH = GRPC_CPP_VERSION_PATCH;
inline constexpr auto value_GRPC_CPP_VERSION_STRING = GRPC_CPP_VERSION_STRING;
}
#undef GRPC_CPP_VERSION_MAJOR
#undef GRPC_CPP_VERSION_MINOR
#undef GRPC_CPP_VERSION_PATCH
#undef GRPC_CPP_VERSION_STRING

export module mcpplibs.mwrap.grpc.macros;

export namespace mcpplibs::mwrap::grpc::macros {
inline constexpr auto GRPC_CPP_VERSION_MAJOR = mwrap_grpc_capture::value_GRPC_CPP_VERSION_MAJOR;
inline constexpr auto GRPC_CPP_VERSION_MINOR = mwrap_grpc_capture::value_GRPC_CPP_VERSION_MINOR;
inline constexpr auto GRPC_CPP_VERSION_PATCH = mwrap_grpc_capture::value_GRPC_CPP_VERSION_PATCH;
inline constexpr auto GRPC_CPP_VERSION_STRING = mwrap_grpc_capture::value_GRPC_CPP_VERSION_STRING;

}
