#!/usr/bin/env python3
"""Check the immutable mwrap release coordinate and bilingual entry pages."""
import argparse
from pathlib import Path
import re
import tomllib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', help='release tag; omitted during PR checks')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    package = tomllib.loads((root / 'mcpp.toml').read_text(encoding="utf-8"))['package']
    assert package['namespace'] == 'mcpplibs.mwrap', package
    assert re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', package['name']), package['name']
    upstream = package['metadata']['mwrap']['upstream-version']
    version = package['version']
    assert version == upstream or re.fullmatch(re.escape(upstream) + r'\.[1-9][0-9]*', version), (version, upstream)
    if args.tag:
        assert args.tag == 'mwrap-' + version, (args.tag, version)
    identity = package['namespace'] + '.' + package['name']
    for name, other in [('README.md', 'README.zh.md'), ('README.zh.md', 'README.md')]:
        text = (root / name).read_text(encoding="utf-8")
        assert len(text.splitlines()) <= 150, name
        assert '](' + other + ')' in '\n'.join(text.splitlines()[:12]), name
        assert identity in text and version in text, name
        assert text.count('```') % 2 == 0, name
    print(identity + '@' + version)


if __name__ == '__main__':
    main()
