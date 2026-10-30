#!/usr/bin/env python3
"""Pin SukiSU-Ultra version info into the vendored drivers/kernelsu/Kbuild.

The kernel tree carries an in-tree copy of the SukiSU-Ultra kernel module, so
the Kbuild must never query git/curl there: ../.git is the KERNEL tree, and a
network query at build time makes every local build slow and non-hermetic.
This script replaces the five detection expressions with literals computed
from the released tag:

    KSU_VERSION   = VERSION_BASE(40000) + LOCAL_COUNT - VERSION_OFFSET(2815)
    VERSION_TAG   = <tag>
    version sha   = <sha8> of the SukiSU-Ultra release commit

The number printed in the release asset name is the formula value itself,
e.g. SukiSU_v4.3.0_41100-release.apk -> 41100 = 40000 + 3915 - 2815, where
3915 is the rev-list count of the tag. LOCAL_COUNT must stay in the bare
count domain (the Kbuild formula adds the base/offset on top), so an
asset-name input is converted here via --count-is asset (default); pass
--count-is bare when the caller already resolved a rev-list count.

Anchors match the landed Kbuild shapes (the git_branch probe exists in
both the $(shell ...) and the v4.3.0 $(if $(KSU_BUILD_REF),...) form); any
further upstream Kbuild restructure breaks them and this script exits 2 so
the calling workflow stops instead of shipping a broken pin.
"""
import argparse
import re
import sys

ap = argparse.ArgumentParser()
ap.add_argument('--kbuild', required=True)
ap.add_argument('--tag', required=True, help='release tag without v prefix, e.g. 4.2.1')
ap.add_argument('--count', required=True,
                help='asset-name value (default) or bare rev-list count, per --count-is')
ap.add_argument('--count-is', choices=('asset', 'bare'), default='asset',
                help='asset: release asset-name value (KSU_VERSION formula); '
                     'bare: rev-list count of the release tag')
ap.add_argument('--sha8', required=True)
ap.add_argument('--branch', default='main')
a = ap.parse_args()
# CI passes the release tag_name verbatim; it may carry the leading v that
# KSU_VERSION_FULL prepends itself, so normalize to the bare version.
a.tag = a.tag[1:] if a.tag.startswith("v") else a.tag

text = open(a.kbuild).read()

# LOCAL_COUNT lives in the bare-count domain: VERSION_BASE + bare - VERSION_OFFSET
# must equal the asset-name number the manager was built with, so convert here.
# Both constants are read from the Kbuild itself - no hard-coded copies to drift.
m_base = re.search(r"^VERSION_BASE[ \t]+:=[ \t]+([0-9]+)[ \t]*$", text, re.M)
m_off = re.search(r"^VERSION_OFFSET[ \t]+:=[ \t]+([0-9]+)[ \t]*$", text, re.M)
if not (m_base and m_off):
    sys.exit("VERSION_BASE/VERSION_OFFSET not found in Kbuild - refusing to pin blindly")
v_base, v_off = int(m_base.group(1)), int(m_off.group(1))
try:
    count_in = int(a.count)
except ValueError:
    sys.exit(f"non-numeric --count: {a.count!r}")
bare_count = count_in - v_base + v_off if a.count_is == "asset" else count_in
if bare_count <= 0:
    sys.exit(f"bare count resolves to {bare_count} "
             f"(--count {count_in}, base {v_base}, offset {v_off}, --count-is {a.count_is}) - nonsense pin")
formula_value = bare_count + v_base - v_off

SUBS = [
    (r"^git_short_sha    = \$\(shell .*$",
     f"git_short_sha    = {a.sha8}"),
    (r"^git_branch       = \$\((?:if|shell) .*$",
     f"git_branch       = {a.branch}"),
    (r"^git_commit_count = \$\(shell .*$",
     f"git_commit_count = {bare_count}"),
    (r"^\$\(shell cd \$\(KSU_SRC\) 2>/dev/null && \[ -f \.\./\.git/shallow \].*$",
     "# vendored in-tree: no shallow fetch, no git query, no network query"),
    (r"^KSU_GITHUB_VER   := \$\(shell .*$",
     f"KSU_GITHUB_VER   := {a.tag}"),
    (r"^GITHUB_COMMITS   := \$\(shell .*$",
     "GITHUB_COMMITS   :="),
]

for pat, repl in SUBS:
    text, n = re.subn(pat, repl, text, flags=re.M)
    if n != 1:
        sys.exit(f"anchor matched {n} times (expected 1): {pat}\n"
                 "Kbuild structure changed - refusing to pin blindly")

marker = ("# Vendored as an in-tree copy of SukiSU-Ultra v{tag} @ {sha8} "
          "(release tag v{tag}, rev-list count {bare}; asset-name value {formula}).\n"
          "# count = rev-list count of the release tag; the manager asset name carries the\n"
          "# formula value VERSION_BASE + count - VERSION_OFFSET (= {formula}),\n"
          "# never the bare count - pair against the formula, not the count line.\n"
          "# Do NOT restore the $(shell git/curl) probes below: this directory\n"
          "# lives inside the kernel git repository, so they would report the\n"
          "# kernel tree's sha/count and hit the network on every build.\n"
          "# {sha8}\n").format(tag=a.tag, sha8=a.sha8, bare=bare_count, formula=formula_value)
text = text.replace("GIT_BIN  := ", marker + "GIT_BIN  := ", 1)

open(a.kbuild, 'w').write(text)
print(f"pinned: tag v{a.tag} rev-list count {bare_count} -> formula value {formula_value} "
      f"(input {count_in} as {a.count_is}) sha {a.sha8} branch {a.branch}")
