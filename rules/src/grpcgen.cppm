// grpcgen — protoc + plugins as an importable build rule.
//
// Consumers write three lines and never see anything below:
//
//   import mcpp;
//   import grpcgen;
//   int main() { return grpcgen::generate_all() ? 0 : 1; }
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
//
// ── WHAT CODEGEN ACTUALLY IS ───────────────────────────────────────────────
//
// Two protoc invocations' worth of work, and only the second is gRPC's:
//
//   --cpp_out=<dir>                  ->  <name>.pb.{h,cc}       protobuf messages
//   --grpc_out=<dir> --plugin=...    ->  <name>.grpc.pb.{h,cc}  gRPC service stubs
//
// This rule issues them as ONE protoc call (protoc accepts both outputs at
// once), but they are two different products from two different projects. That
// is why a gRPC project needs `compat.protobuf`'s protoc at all — the message
// half is protobuf's, not gRPC's, and no gRPC-only tool can produce it. The
// service stubs `#include` the message headers, so the two are not separable
// in C++.
//
// ── THREE LAYERS, EACH THE NEXT ONE'S DEFAULTS ─────────────────────────────
//
//   L0  generate_all()                     — the whole directory, no arguments
//   L1  generate_all({.imports=…, .mock=…, .protoc_args=…})
//   L2  generate_all({.plugins={cpp(), my_plugin}})
//   L3  auto e = plan_all(); /* edit */ ; submit(e);
//
// They are not alternatives: `generate_all(opt)` IS `submit(plan_all(opt))`,
// and `.grpc = true` IS `.plugins = {cpp()}`. A consumer that needs L3 keeps
// everything the rule already solved — locating the well-known types, creating
// output subdirectories for nested .proto, computing the input set — instead of
// hand-writing a build.mcpp that reimplements them and then drifts.
//
// WHAT THIS RULE DOES NOT DO: print command lines. mcpp already writes every
// action's full argv into build.ninja (`rule mcpp_action_N / command = …`),
// recoverable with `ninja -t commands <output>`. A second source of that truth
// would only drift. The rule owns the other half — WHICH KNOBS produced the
// command — and puts it in each edge's description.
//
// See .agents/docs/2026-08-06-grpcgen-layered-control-design.md.
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

// ── L2: a protoc plugin ────────────────────────────────────────────────────
//
// One plugin is four things, and the fourth is not optional: mcpp has to know
// which files the edge PRODUCES to put the .cc in the compile set and keep the
// .h out of it. A plugin that cannot say what it emits cannot be a build-graph
// node at all.
struct plugin {
    // The protoc name: drives `--<name>_out=` and `--plugin=protoc-gen-<name>=`.
    std::string name;
    // Absolute path to the generator executable. Empty = unresolved; the rule
    // reports it by name rather than letting protoc fail with its own wording.
    std::string binary;
    // Plugin parameters, joined with ',' and prefixed onto the output dir —
    // protoc's `--x_out=a=1,b=2:<dir>` grammar.
    std::vector<std::string> params;
    // Suffixes appended to the .proto's stem, e.g. ".grpc.pb.cc".
    std::vector<std::string> suffixes;
};

// The gRPC C++ plugin, resolved from the dependency that provides it.
//
// `mock = true` adds gRPC's own `generate_mock_code=true`, whose extra output
// is `<stem>_mock.grpc.pb.h`. It is a PLUGIN parameter, not a protoc flag —
// exactly the kind of spelling a consumer should not have to know, which is
// why it is a named option rather than something to hand-write into
// `protoc_args`.
plugin cpp(bool mock = false) {
    plugin p;
    p.name   = "grpc";
    const char* bin = mcpp::dep_bin("grpc-plugin", "grpc_cpp_plugin");
    p.binary = bin ? bin : "";
    p.suffixes = { ".grpc.pb.cc", ".grpc.pb.h" };
    if (mock) {
        p.params.push_back("generate_mock_code=true");
        p.suffixes.push_back("_mock.grpc.pb.h");
    }
    return p;
}

struct options {
    // Where the .proto files live, relative to the consumer's manifest.
    std::string_view proto_dir = "proto";
    // Also run the gRPC C++ plugin. Sugar for `.plugins = {cpp(mock)}`; ignored
    // when `plugins` is set explicitly. Set false for a protobuf-only project,
    // which then needs neither the plugin dependency nor its tool.
    bool grpc = true;
    // Additional trees to GENERATE from, each also becoming an `-I` root.
    // Names keep their path relative to their own root, so
    // ../shared-proto/common/types.proto emits common/types.pb.{h,cc}.
    //
    // Distinct from `imports` on purpose, and the distinction is not academic:
    // protoc emits `#include "common/types.pb.h"` into any file that imports
    // that .proto, so a shared tree reached only through `-I` yields a header
    // nobody produced — and the failure surfaces as a missing include far from
    // its cause. Use this when the shared .proto's code is yours to build.
    std::vector<std::string> extra_dirs;
    // Extra `-I` roots, SEARCH ONLY — no code is generated for what they
    // contain. Use this when the generated code comes from somewhere else
    // (a package you already link that ships it). If you need the code too,
    // use `extra_dirs`.
    std::vector<std::string> imports;
    // Generate gRPC's mock classes for unit tests. See cpp().
    bool mock = false;
    // Escape hatch for protoc flags this rule does not model, e.g.
    // "--experimental_allow_proto3_optional".
    std::vector<std::string> protoc_args;
    // L2: the full plugin list. Non-empty overrides `grpc`/`mock`.
    std::vector<plugin> plugins;
};

// ── L3: a planned edge, before it is submitted ─────────────────────────────
//
// Owns its strings, unlike `mcpp::action` whose id/description are raw
// pointers — a planned edge outlives the expression that built it.
struct edge {
    std::string              id;
    std::string              description;
    std::vector<std::string> command;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    edge& arg(std::string a)    { command.push_back(std::move(a)); return *this; }
    edge& input(std::string p)  { inputs.push_back(std::move(p));  return *this; }
    edge& output(std::string p) { outputs.push_back(std::move(p)); return *this; }
};

namespace detail {

std::vector<plugin> resolve_plugins(const options& opt) {
    if (!opt.plugins.empty()) return opt.plugins;
    if (!opt.grpc) return {};
    return { cpp(opt.mock) };
}

// "protoc:echo (+grpc +mock, -I proto -I ../shared/proto)"
//
// The description is what every build prints, so it is where "which knobs
// produced this" belongs. The command itself is in build.ninja; see the module
// header for why the rule does not duplicate it.
std::string describe(const std::string& name,
                     const std::vector<plugin>& plugins,
                     const std::vector<std::string>& userIncs,
                     const std::string& root) {
    std::string s = "protoc:" + name + " (";
    if (plugins.empty()) {
        s += "messages-only";
    } else {
        bool first = true;
        for (const auto& p : plugins) {
            if (!first) s += " ";
            first = false;
            s += "+" + p.name;
            for (const auto& q : p.params) {
                // `generate_mock_code=true` reads as `+mock` — the option's
                // name, not protoc's spelling of it.
                if (q == "generate_mock_code=true") s += " +mock";
            }
        }
    }
    // Only the roots the AUTHOR chose. The well-known-types directory is
    // always present and is not a knob, and absolute store paths would bury
    // the part worth reading. Rendered relative to the manifest so the string
    // matches what was written in build.mcpp.
    for (const auto& i : userIncs) {
        // `..`-relative is kept: it is what the author wrote in build.mcpp,
        // and it is far shorter than the store path it would otherwise print.
        auto rel = std::filesystem::path(i).lexically_relative(root);
        auto shown = rel.empty() ? i : rel.generic_string();
        s += (i == userIncs.front() ? ", -I" : " -I") + shown;
    }
    s += ")";
    return s;
}

}  // namespace detail

// Build the edges for the named .proto files WITHOUT submitting them.
//
// Names are given WITHOUT the extension, relative to `opt.proto_dir` —
// "helloworld" means <proto_dir>/helloworld.proto.
//
// Returns an empty vector after printing a diagnostic.
// One planned .proto: which generation ROOT it came from, and its name
// relative to that root. Two roots can contribute files with the same relative
// name only if the author arranged it, exactly as with protoc's own -I list.
struct entry { std::string root, name; };

std::vector<edge> plan_entries(std::vector<entry> protos, options opt = {}) {
    const std::string root = mcpp::manifest_dir();
    const std::string out  = mcpp::out_dir();
    if (root.empty() || out.empty()) {
        std::println(std::cerr,
            "grpcgen: no mcpp build context — this runs from build.mcpp");
        return {};
    }
    if (protos.empty()) {
        std::println(std::cerr, "grpcgen: no .proto files to plan");
        return {};
    }

    const char* protoc = mcpp::dep_bin("protobuf", "protoc");
    if (!protoc || !*protoc) {
        std::println(std::cerr,
            "grpcgen: no protoc. It generates the PROTOBUF half of the output "
            "(.pb.cc/.pb.h), which is protobuf's, not gRPC's — so it is "
            "declared on protobuf:\n"
            "  compat.protobuf = {{ version = \"35.1\", tools = [\"protoc\"] }}\n"
            "  (or simply  grpc = {{ version = \"...\", features = [\"codegen\"] }},"
            " which hands you all of it)");
        return {};
    }

    auto plugins = detail::resolve_plugins(opt);
    for (const auto& p : plugins) {
        if (!p.binary.empty()) continue;
        std::println(std::cerr,
            "grpcgen: plugin '{}' has no executable. For the built-in gRPC C++ "
            "plugin declare:\n"
            "  grpc-plugin = {{ version = \"...\", tools = "
            "[\"grpc_cpp_plugin\"] }}\n"
            "  (or grpcgen::generate_all({{.grpc = false}}) for a "
            "protobuf-only project)", p.name);
        return {};
    }

    const std::string wkt = detail::well_known_types_dir();
    if (wkt.empty()) {
        std::println(std::cerr,
            "grpcgen: cannot locate the well-known .proto files inside the "
            "protobuf package");
        return {};
    }

    auto abs_under_root = [&](const std::string& v) {
        std::filesystem::path p{v};
        return p.is_absolute() ? p.generic_string()
                               : (std::filesystem::path(root) / p)
                                     .lexically_normal().generic_string();
    };
    const std::string protoDir = abs_under_root(std::string(opt.proto_dir));

    // Include roots, in the order protoc sees them: the generation roots (own
    // tree first), then search-only roots, then the well-known types.
    std::vector<std::string> incs{ protoDir };
    for (const auto& d : opt.extra_dirs) incs.push_back(abs_under_root(d));
    for (const auto& i : opt.imports)    incs.push_back(abs_under_root(i));
    incs.push_back(wkt);

    // Every .proto is an input of every action. A .proto that imports a sibling
    // has a real dependency this rule does not parse, and regenerating a little
    // too eagerly is far better than silently stale stubs.
    std::vector<std::string> inputs;
    inputs.reserve(protos.size());
    for (const auto& e : protos)
        inputs.push_back(std::format("{}/{}.proto", e.root, e.name));

    std::vector<edge> edges;
    edges.reserve(protos.size());

    for (const auto& ent : protos) {
        const std::string& name = ent.name;
        const std::string src  = std::format("{}/{}.proto", ent.root, name);
        const std::string base = std::format("{}/{}", out, name);

        edge e;
        e.id          = std::format("protoc:{}", name);
        // Only the author-chosen roots: incs also carries the well-known-types
        // directory, which is constant and not worth printing every build.
        e.description = detail::describe(
            name, plugins,
            std::vector<std::string>(incs.begin(), incs.end() - 1), root);

        e.arg(protoc);
        for (const auto& i : incs) e.arg("-I" + i);
        e.arg("--cpp_out=" + out);

        for (const auto& p : plugins) {
            std::string spec;
            for (const auto& q : p.params) {
                if (!spec.empty()) spec += ",";
                spec += q;
            }
            // protoc's grammar: `--x_out=<params>:<dir>`; the colon is only
            // present when there are params.
            e.arg("--" + p.name + "_out=" + (spec.empty() ? out : spec + ":" + out));
            e.arg("--plugin=protoc-gen-" + p.name + "=" + p.binary);
        }
        for (const auto& a : opt.protoc_args) e.arg(a);
        e.arg(src);

        for (const auto& in : inputs) e.input(in);

        // protoc mirrors the .proto's relative path under --cpp_out and does
        // NOT create the intermediate directories. A flat proto/ never notices;
        // proto/sub/x.proto fails with "No such file or directory" from inside
        // protoc, which reads as a codegen bug rather than a missing mkdir.
        {
            std::error_code mkec;
            std::filesystem::create_directories(
                std::filesystem::path(base).parent_path(), mkec);
        }
        // The headers are declared as outputs because they must be PRODUCED by
        // this edge; mcpp knows a header is not a translation unit and keeps
        // them out of the compile set.
        e.output(base + ".pb.cc").output(base + ".pb.h");
        for (const auto& p : plugins)
            for (const auto& s : p.suffixes) e.output(base + s);

        edges.push_back(std::move(e));
    }
    return edges;
}

// Every .proto under `opt.proto_dir`, without naming any of them — planned but
// not submitted.
//
// Safe only because the engine can express "my output depends on which files
// are here" (`rerun_if_changed_glob`, mcpp 2026.8.6.2+). Before that, a build
// program that globbed did not re-run when a .proto was added — no declared
// file's hash had changed — so the new file was silently never generated,
// which is worse than making the author list names. That is why this
// repository shipped the explicit list first.
//
// Names keep their subdirectory, so proto/sub/x.proto generates sub/x.pb.cc.
// Build the edges for the named .proto files WITHOUT submitting them.
//
// Names are given WITHOUT the extension, relative to `opt.proto_dir` —
// "helloworld" means <proto_dir>/helloworld.proto. Files under `extra_dirs`
// are not addressable this way; use plan_all() for those.
//
// Returns an empty vector after printing a diagnostic.
std::vector<edge> plan(std::vector<std::string> protos, options opt = {}) {
    const std::string root = mcpp::manifest_dir();
    if (root.empty()) {
        std::println(std::cerr,
            "grpcgen: no mcpp build context — this runs from build.mcpp");
        return {};
    }
    std::filesystem::path pd{std::string(opt.proto_dir)};
    const std::string base = pd.is_absolute()
        ? pd.generic_string()
        : (std::filesystem::path(root) / pd).lexically_normal().generic_string();
    std::vector<entry> entries;
    entries.reserve(protos.size());
    for (auto& n : protos) entries.push_back({ base, std::move(n) });
    return plan_entries(std::move(entries), std::move(opt));
}

std::vector<edge> plan_all(options opt = {}) {
    namespace fs = std::filesystem;
    const std::string root = mcpp::manifest_dir();
    if (root.empty()) {
        std::println(std::cerr,
            "grpcgen: no mcpp build context — this runs from build.mcpp");
        return {};
    }
    // Every generation root: the project's own tree, then `extra_dirs`.
    std::vector<std::string> roots{ std::string(opt.proto_dir) };
    for (const auto& d : opt.extra_dirs) roots.push_back(d);

    std::vector<entry> entries;
    for (const auto& r : roots) {
        // One glob per root — a .proto appearing in ANY of them must re-run
        // this program, and only the path SET is fingerprinted (contents are
        // covered by the edges' own inputs).
        const std::string pattern = r + "/**/*.proto";
        mcpp::rerun_if_changed_glob(pattern.c_str());

        fs::path rp{r};
        const fs::path dir = rp.is_absolute() ? rp
                                              : (fs::path(root) / rp).lexically_normal();
        std::error_code ec;
        if (!fs::exists(dir, ec)) {
            std::println(std::cerr,
                "grpcgen::plan_all(): no '{}' directory under {}", r, root);
            return {};
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
        // Sorted so the declared edge set is stable run to run — an unstable
        // order would churn build.ninja for no reason.
        std::ranges::sort(names);
        for (auto& n : names)
            entries.push_back({ dir.generic_string(), std::move(n) });
    }
    if (entries.empty()) {
        std::println(std::cerr,
            "grpcgen::plan_all(): no .proto files under {}", opt.proto_dir);
        return {};
    }
    return plan_entries(std::move(entries), std::move(opt));
}

// Hand the planned edges to mcpp, and declare where the generated headers live.
//
// That include dir is PRIVATE to the consuming package by design — one a
// package's own consumers must see belongs in its manifest, not in a build
// program.
bool submit(const std::vector<edge>& edges) {
    if (edges.empty()) return false;
    for (const auto& e : edges) {
        mcpp::action a;
        a.id          = e.id.c_str();
        a.role        = "source";
        a.description = e.description.c_str();
        for (const auto& c : e.command) a.arg(c.c_str());
        for (const auto& i : e.inputs)  a.input(i.c_str());
        for (const auto& o : e.outputs) a.output(o.c_str());
        a.submit();
    }
    mcpp::include_dir(mcpp::out_dir());
    return true;
}

// Declare one codegen edge per named .proto. `plan` + `submit`, with nothing
// in between — the two-call form exists so a consumer can put something there.
bool generate(std::vector<std::string> protos, options opt = {}) {
    return submit(plan(std::move(protos), std::move(opt)));
}

// Every .proto under `opt.proto_dir`. The form the greeter template uses.
bool generate_all(options opt = {}) {
    return submit(plan_all(std::move(opt)));
}

}  // namespace grpcgen
