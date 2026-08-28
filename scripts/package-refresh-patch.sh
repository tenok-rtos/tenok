#!/usr/bin/env bash
#
# Capture the current state of a package submodule as a patch of its series.
#
# The workflow is: run package-prepare.sh, edit the submodule in place until it
# builds, then run this script to turn the edits into a patch. The submodule
# stays pinned to its release commit, and every Tenok change lives in the patch
# repository.
#
#     ./scripts/package-refresh-patch.sh busybox 0001-name-of-the-change
#
set -e

if [ $# -lt 2 ]; then
    echo "usage: $0 <package> <patch-name-without-extension> [path]..."
    echo
    echo "Without a path the whole submodule is captured, which is only right"
    echo "for the last patch of the series. Name the paths a patch owns when"
    echo "there are already patches applied on top of the pinned commit."
    echo
    echo "A patch is taken against the pinned commit and not against the"
    echo "patches before it, so a file belongs to exactly one patch of the"
    echo "series. Two patches naming the same file will not both apply."
    exit 1
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
package="$1"
SOURCE="${ROOT}/lib/${package}"
OUTPUT="${ROOT}/lib/package-patches/${package}/patches/$2.patch"
shift 2

# Only modifications to tracked files are captured. The build leaves generated
# headers and objects behind in the submodule and they must not end up in a
# patch.
git -C "${SOURCE}" diff -- "$@" >"${OUTPUT}"

if [ ! -s "${OUTPUT}" ]; then
    rm -f "${OUTPUT}"
    echo "lib/${package} has no change, nothing was written"
    exit 1
fi

echo "wrote ${OUTPUT} ($(grep -c '^+++' "${OUTPUT}") files)"
