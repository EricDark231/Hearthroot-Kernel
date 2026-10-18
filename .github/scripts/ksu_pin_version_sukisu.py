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

The count for a released tag is printed in the release asset name, e.g.
SukiSU_v4.2.1_40994-release.apk -> count 40994.

Anchors are exact against the landed v4.2.1 Kbuild; any upstream Kbuild
restructure changes them and this script exits 2 so the calling workflow
stops instead of shipping a broken pin.
"""
import argparse
import re
import sys

ap = argparse.ArgumentParser()
ap.add_argument('--kbuild', required=True)
ap.add_argument('--tag', required=True, help='release tag without v prefix, e.g. 4.2.1')
ap.add_argument('--count', required=True, help='commit count baked into the release APK name')
ap.add_argument('--sha8', required=True)
ap.add_argument('--branch', default='main')
a = ap.parse_args()

text = open(a.kbuild).read()

SUBS = [
    (r"^git_short_sha    = \$\(shell .*$",
     f"git_short_sha    = {a.sha8}"),
    (r"^git_branch       = \$\(shell .*$",
     f"git_branch       = {a.branch}"),
    (r"^git_commit_count = \$\(shell .*$",
     f"git_commit_count = {a.count}"),
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
          "(release tag v{tag}, count {count}).\n"
          "# Do NOT restore the $(shell git/curl) probes below: this directory\n"
          "# lives inside the kernel git repository, so they would report the\n"
          "# kernel tree's sha/count and hit the network on every build.\n"
          "# {sha8}\n").format(tag=a.tag, sha8=a.sha8, count=a.count)
text = text.replace("GIT_BIN  := ", marker + "GIT_BIN  := ", 1)

open(a.kbuild, 'w').write(text)
print(f"pinned: tag v{a.tag} count {a.count} sha {a.sha8} branch {a.branch}")
