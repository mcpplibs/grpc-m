#!/usr/bin/env python3
"""Regenerate the gRPC source arrays in mcpp.toml from upstream's CMakeLists.txt.

The library source list in mcpp.toml is not hand-maintained: it is upstream's
own, transcribed. This script is what makes that claim checkable — point it at
a gRPC checkout of the pinned tag and it rewrites the two arrays in place, so a
version bump is a mechanical, reviewable diff rather than an act of faith.

    tools/gen_sources.py --grpc /path/to/grpc-1.83.0 [--check]

--check exits non-zero if mcpp.toml would change, which is what CI runs to
prove the vendored tree and the manifest have not drifted apart.

What it takes and why:

  * the union of add_library(gpr), add_library(grpc), add_library(grpc++) and
    add_library(address_sorting) — the four upstream targets that make up a
    static gRPC C++ build.
  * MINUS src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c,
    which is byte-for-byte identical to the bootstrap copy compat.protobuf's
    `upb` feature compiles; building both is a duplicate-symbol failure.
  * with the 7 c-ares resolver TUs split into [features.ares].sources.

The *_posix / *_windows variants stay in unconditionally, exactly as upstream
lists them: each self-gates on GPR_POSIX_* / GPR_WINDOWS (and
ADDRESS_SORTING_POSIX / _WINDOWS) and compiles to an empty TU elsewhere.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

VENDOR = "third_party/grpc-1.83.0"
TARGETS = ("gpr", "grpc", "grpc++", "address_sorting")
DUP_DESCRIPTOR = "src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c"

BEGIN_CORE = "sources = [\n"
BEGIN_ARES = "sources = [\n"


def target_sources(cmake_text: str, target: str) -> list[str]:
    m = re.search(
        r"\nadd_library\(" + re.escape(target) + r"\s+(.*?)\n\)\n", cmake_text, re.S
    )
    if not m:
        sys.exit(f"error: add_library({target} ...) not found in CMakeLists.txt")
    return [
        line.strip()
        for line in m.group(1).splitlines()
        if line.strip().endswith((".cc", ".c", ".cpp"))
    ]


def collect(grpc_root: pathlib.Path) -> tuple[list[str], list[str]]:
    cmake = (grpc_root / "CMakeLists.txt").read_text()
    seen: set[str] = set()
    ordered: list[str] = []
    for t in TARGETS:
        for f in target_sources(cmake, t):
            if f not in seen:
                seen.add(f)
                ordered.append(f)

    if DUP_DESCRIPTOR not in seen:
        sys.exit(
            f"error: expected {DUP_DESCRIPTOR} in upstream's list — the "
            "duplicate-symbol exclusion may no longer apply; re-check before "
            "removing it"
        )

    ares = [f for f in ordered if "/c_ares/" in f or f.endswith("ares_resolver.cc")]
    if not ares:
        sys.exit("error: no c-ares TUs found — the `ares` feature would be empty")

    core = [f for f in ordered if f != DUP_DESCRIPTOR and f not in set(ares)]
    return core, ares


LIB_ROOT_PREAMBLE = (
    "    # The lib root: the C++23 module interface. mcpp resolves the lib root\n"
    "    # of a `kind = \"lib\"` package as src/<package tail>.cppm, so this file\n"
    "    # is both the module and what satisfies that convention.\n"
    '    "src/grpc.cppm",\n'
)


def render(paths: list[str], *, with_lib_root: bool = False) -> str:
    body = "".join(f'    "{VENDOR}/{p}",\n' for p in paths)
    return (LIB_ROOT_PREAMBLE + body) if with_lib_root else body


def replace_array(text: str, section_marker: str, body: str) -> str:
    """Replace the first `sources = [ ... ]` array after `section_marker`."""
    start = text.index(section_marker)
    arr = text.index(BEGIN_CORE, start) + len(BEGIN_CORE)
    end = text.index("]\n", arr)
    return text[:arr] + body + text[end:]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--grpc", required=True, type=pathlib.Path,
                    help="path to an upstream gRPC checkout of the pinned tag")
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if mcpp.toml would change")
    args = ap.parse_args()

    repo = pathlib.Path(__file__).resolve().parent.parent
    manifest = repo / "mcpp.toml"
    original = manifest.read_text()

    core, ares = collect(args.grpc)
    updated = replace_array(original, "\n[build]\n", render(core, with_lib_root=True))
    updated = replace_array(updated, "\n[features.ares]\n", render(ares))

    if args.check:
        if updated != original:
            print("mcpp.toml is out of date — run tools/gen_sources.py", file=sys.stderr)
            return 1
        print(f"mcpp.toml is up to date ({len(core)} sources + {len(ares)} ares)")
        return 0

    manifest.write_text(updated)
    print(f"mcpp.toml updated: {len(core)} sources + {len(ares)} ares")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
