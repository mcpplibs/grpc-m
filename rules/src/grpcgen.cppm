// grpcgen — protoc + grpc_cpp_plugin as an importable build rule.
//
// Consumers write three lines and never see anything below:
//
//   import mcpp;
//   import grpcgen;
//   int main() { return grpcgen::generate({"helloworld"}) ? 0 : 1; }
//
// `host-module = true` makes mcpp compile this interface in the same command as
// the consumer's build.mcpp, which is what makes the BMI importable there and
// what lets this file use `import mcpp;` — the typed wrapper over the `mcpp:`
// directive protocol — rather than hand-printing JSON.
//
// Requires mcpp >= 2026.8.6.2 for generate_all() (see there) and >= 2026.8.5.2
// for everything else. Both of those properties are fixes in that
// release: before it, a rule was compiled before the std module existed
// (`module 'std' not found`) and was ALSO compiled as an ordinary library of
// the consumer, where the `mcpp` module does not exist.
export module grpcgen;

import std;
import mcpp;

namespace grpcgen::detail {

// protoc does not embed the well-known types: `import
// "google/protobuf/timestamp.proto"` — and Duration, Any, Empty, Struct, which
// real services use constantly — is read from disk. They ship inside the
// protobuf package the consumer already depends on, so the path is derivable
// and nobody has to install anything.
//
// Found by looking for the directory that actually holds descriptor.proto,
// rather than by assuming the tarball's wrap-directory name: that name is a
// packaging artifact and not part of any contract.
std::string well_known_types_dir() {
    namespace fs = std::filesystem;
    const std::string base = mcpp::dep_dir("protobuf");
    if (base.empty()) return {};
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(base, ec)) {
        const fs::path src = e.path() / "src";
        if (fs::exists(src / "google" / "protobuf" / "descriptor.proto", ec))
            return src.generic_string();
    }
    return {};
}

}  // namespace grpcgen::detail

export namespace grpcgen {

struct options {
    // Where the .proto files live, relative to the consumer's manifest.
    std::string_view proto_dir = "proto";
    // Also run grpc_cpp_plugin. Set false for a protobuf-only project, which
    // then needs neither the plugin dependency nor its tool.
    bool grpc = true;
};

// Declare one codegen edge per .proto. Names are given WITHOUT the extension,
// relative to `opt.proto_dir` — "helloworld" means <proto_dir>/helloworld.proto.
//
// Returns false after printing a diagnostic; a build program should propagate
// that as a non-zero exit.
bool generate(std::vector<std::string> protos, options opt = {}) {
    const std::string root = mcpp::manifest_dir();
    const std::string out  = mcpp::out_dir();
    if (root.empty() || out.empty()) {
        std::println(std::cerr,
            "grpcgen: no mcpp build context — this runs from build.mcpp");
        return false;
    }
    if (protos.empty()) {
        std::println(std::cerr, "grpcgen::generate() called with no .proto files");
        return false;
    }

    const char* protoc = mcpp::dep_bin("protobuf", "protoc");
    if (!protoc || !*protoc) {
        std::println(std::cerr,
            "grpcgen: no protoc. Declare it in mcpp.toml:\n"
            "  compat.protobuf = {{ version = \"35.1\", tools = [\"protoc\"] }}");
        return false;
    }

    const char* plugin = "";
    if (opt.grpc) {
        plugin = mcpp::dep_bin("grpc-plugin", "grpc_cpp_plugin");
        if (!plugin || !*plugin) {
            std::println(std::cerr,
                "grpcgen: no grpc_cpp_plugin. Declare it in mcpp.toml:\n"
                "  grpc-plugin = {{ version = \"...\", tools = "
                "[\"grpc_cpp_plugin\"] }}\n"
                "  (or call grpcgen::generate(..., {{.grpc = false}}) for a "
                "protobuf-only project)");
            return false;
        }
    }

    const std::string wkt = detail::well_known_types_dir();
    if (wkt.empty()) {
        std::println(std::cerr,
            "grpcgen: cannot locate the well-known .proto files inside the "
            "protobuf package");
        return false;
    }

    const std::string protoDir = root + "/" + std::string(opt.proto_dir);

    // Every .proto is an input of every action. A .proto that imports a sibling
    // has a real dependency this rule does not parse, and regenerating a little
    // too eagerly is far better than silently stale stubs.
    std::vector<std::string> inputs;
    inputs.reserve(protos.size());
    for (const auto& n : protos)
        inputs.push_back(std::format("{}/{}.proto", protoDir, n));

    for (const auto& name : protos) {
        const std::string src  = std::format("{}/{}.proto", protoDir, name);
        const std::string base = std::format("{}/{}", out, name);

        // The work is DECLARED, not done. Running protoc here would re-run it
        // on every prepare, serially, and report failure as "build.mcpp exited
        // 1". As an action it is an edge in the build graph: it re-runs when
        // its inputs change, in parallel, and a failure is attributed to the
        // edge that produced it.
        //
        // The headers are declared as outputs because they must be PRODUCED by
        // this edge; mcpp knows a header is not a translation unit and keeps
        // them out of the compile set.
        mcpp::action gen;
        const std::string id = std::format("protoc:{}", name);
        gen.id          = id.c_str();
        gen.role        = "source";
        gen.description = id.c_str();

        const std::string incProto = "-I" + protoDir;
        const std::string incWkt   = "-I" + wkt;
        const std::string cppOut   = "--cpp_out=" + out;
        gen.arg(protoc).arg(incProto.c_str()).arg(incWkt.c_str())
           .arg(cppOut.c_str());

        const std::string grpcOut   = "--grpc_out=" + out;
        const std::string pluginArg = std::string("--plugin=protoc-gen-grpc=") + plugin;
        if (opt.grpc) gen.arg(grpcOut.c_str()).arg(pluginArg.c_str());
        gen.arg(src.c_str());

        for (const auto& in : inputs) gen.input(in.c_str());

        // protoc mirrors the .proto's relative path under --cpp_out and does
        // NOT create the intermediate directories. A flat proto/ never notices;
        // proto/sub/x.proto fails with "No such file or directory" from inside
        // protoc, which reads as a codegen bug rather than a missing mkdir.
        {
            std::error_code mkec;
            std::filesystem::create_directories(
                std::filesystem::path(base).parent_path(), mkec);
        }
        const std::string pbcc = base + ".pb.cc", pbh = base + ".pb.h";
        gen.output(pbcc.c_str()).output(pbh.c_str());
        const std::string gcc_ = base + ".grpc.pb.cc", gh = base + ".grpc.pb.h";
        if (opt.grpc) gen.output(gcc_.c_str()).output(gh.c_str());
        gen.submit();
    }

    // Where the generated headers live. PRIVATE to the consuming package by
    // design — an include dir that a package's own consumers must see belongs
    // in its manifest, not in a build program.
    mcpp::include_dir(out.c_str());
    return true;
}

// Every .proto under `opt.proto_dir`, without naming any of them.
//
// This is the form the greeter template uses, and it is only SAFE because the
// engine can express "my output depends on which files are here"
// (`rerun_if_changed_glob`, mcpp 2026.8.6.2+). Before that, a build program
// that globbed did not re-run when a .proto was added — no declared file's
// hash had changed — so the new file was silently never generated, which is
// worse than making the author list names. That is why this repository shipped
// the explicit list first.
//
// Names are relative to `opt.proto_dir` and keep their subdirectory, so
// proto/sub/x.proto generates sub/x.pb.cc and is imported as "sub/x.proto".
bool generate_all(options opt = {}) {
    namespace fs = std::filesystem;
    const std::string root = mcpp::manifest_dir();
    if (root.empty()) {
        std::println(std::cerr,
            "grpcgen: no mcpp build context — this runs from build.mcpp");
        return false;
    }
    const std::string pattern = std::string(opt.proto_dir) + "/**/*.proto";
    mcpp::rerun_if_changed_glob(pattern.c_str());

    const fs::path dir = fs::path(root) / opt.proto_dir;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        std::println(std::cerr,
            "grpcgen::generate_all(): no '{}' directory under {}",
            opt.proto_dir, root);
        return false;
    }
    std::vector<std::string> names;
    for (auto const& e : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file(ec)) continue;
        if (e.path().extension() != ".proto") continue;
        auto rel = e.path().lexically_relative(dir);
        rel.replace_extension();
        names.push_back(rel.generic_string());
    }
    if (names.empty()) {
        std::println(std::cerr,
            "grpcgen::generate_all(): no .proto files under {}", dir.string());
        return false;
    }
    // Sorted so the declared edge set is stable run to run — an unstable order
    // would churn build.ninja for no reason.
    std::ranges::sort(names);
    return generate(std::move(names), opt);
}

}  // namespace grpcgen
