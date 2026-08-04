// grpc — a C++23 module interface over gRPC's public C++ API.
//
// USAGE ORDER MATTERS. `import grpc;` goes LAST, after every textual #include
// in the translation unit — the standard library ones included:
//
//     #include <string>                // std headers first
//     #include "myservice.grpc.pb.h"   // protoc output too
//     import grpc;                     // the module last
//
// The reason is the global module fragment below. It pulls <grpcpp/grpcpp.h>,
// which pulls most of the standard library, and all of it lands in this
// module's BMI. A textual #include of the same headers AFTER the import
// therefore delivers a second copy: GCC reports "redefinition of
// std::__is_constant_evaluated" and a wall of <limits> conflicts for the
// import-first order, and "ambiguous overload for operator==" on std::string
// when only a std header follows. Put the import last and everything resolves,
// because these entities belong to the GLOBAL module (`export using` on
// global-module-fragment declarations re-exports them rather than creating new
// ones), so the textual and imported views are the SAME entities. Verified
// both ways round with gcc 16.1.0.
//
// This is not a constraint this package invented: gRPC's code generator emits
// HEADERS that textually include <grpcpp/...>, so any real gRPC program mixes
// the two worlds no matter what.
//
// So this module is the ergonomic surface for the parts of gRPC you write by
// hand — servers, channels, credentials, contexts — while generated stubs stay
// header-included, which is what upstream's codegen requires.
//
// The export list is deliberately the PUBLIC API only. gRPC declares a great
// deal inside `namespace grpc` that is implementation detail (`grpc::internal`,
// the CallOp* machinery, the *Impl types); scraping the headers wholesale would
// export ~280 names, most of which no user should touch. Every name below is
// validated by the compiler: `export using ::grpc::X;` fails to build if X does
// not exist, so this list cannot silently drift away from the vendored gRPC.
module;

#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/ext/health_check_service_server_builder_option.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/resource_quota.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/sync_stream.h>

export module grpc;

export namespace grpc {

// ── status ───────────────────────────────────────────────────────────────
using ::grpc::Status;
using ::grpc::StatusCode;

// ── channel / client ─────────────────────────────────────────────────────
using ::grpc::Channel;
using ::grpc::ChannelArguments;
using ::grpc::ChannelInterface;
using ::grpc::ClientContext;
using ::grpc::CreateChannel;
using ::grpc::CreateCustomChannel;

// ── server ───────────────────────────────────────────────────────────────
using ::grpc::CallbackServerContext;
using ::grpc::Server;
using ::grpc::ServerBuilder;
using ::grpc::ServerBuilderOption;
using ::grpc::ServerContext;
using ::grpc::ServerContextBase;
using ::grpc::ServerInterface;
using ::grpc::Service;

// ── completion queues / async ────────────────────────────────────────────
using ::grpc::Alarm;
using ::grpc::CompletionQueue;
using ::grpc::ServerCompletionQueue;

// ── streaming ────────────────────────────────────────────────────────────
using ::grpc::ClientReader;
using ::grpc::ClientReaderWriter;
using ::grpc::ClientWriter;
using ::grpc::ServerReader;
using ::grpc::ServerReaderWriter;
using ::grpc::ServerWriter;

// ── credentials ──────────────────────────────────────────────────────────
using ::grpc::AccessTokenCredentials;
using ::grpc::CallCredentials;
using ::grpc::ChannelCredentials;
using ::grpc::CompositeCallCredentials;
using ::grpc::CompositeChannelCredentials;
using ::grpc::GoogleDefaultCredentials;
using ::grpc::InsecureChannelCredentials;
using ::grpc::InsecureServerCredentials;
using ::grpc::MetadataCredentialsFromPlugin;
using ::grpc::MetadataCredentialsPlugin;
using ::grpc::ServerCredentials;
using ::grpc::SslCredentials;
using ::grpc::SslCredentialsOptions;
using ::grpc::SslServerCredentials;
using ::grpc::SslServerCredentialsOptions;

// ── auth ─────────────────────────────────────────────────────────────────
using ::grpc::AuthContext;
using ::grpc::AuthMetadataProcessor;
using ::grpc::AuthProperty;
using ::grpc::AuthPropertyIterator;

// ── payload ──────────────────────────────────────────────────────────────
using ::grpc::ByteBuffer;
using ::grpc::Slice;

// ── health checking / resources ──────────────────────────────────────────
using ::grpc::EnableDefaultHealthCheckService;
using ::grpc::HealthCheckServiceInterface;
using ::grpc::HealthCheckServiceServerBuilderOption;
using ::grpc::ResourceQuota;

// ── library ──────────────────────────────────────────────────────────────
using ::grpc::Version;

}  // namespace grpc

// gRPC's connectivity state is a C enum in the GLOBAL namespace, and it is
// part of the public C++ surface: it is what ChannelInterface::GetState()
// returns. Exporting the type alone would not be enough — an unscoped enum's
// enumerators do not follow a using-declaration of the type — so the five
// enumerators are named individually. Without them a caller could hold the
// return value but never compare it to anything.
export using ::grpc_connectivity_state;
export using ::GRPC_CHANNEL_IDLE;
export using ::GRPC_CHANNEL_CONNECTING;
export using ::GRPC_CHANNEL_READY;
export using ::GRPC_CHANNEL_TRANSIENT_FAILURE;
export using ::GRPC_CHANNEL_SHUTDOWN;
