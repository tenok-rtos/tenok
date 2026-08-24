#!/usr/bin/env bash
#
# Capture the current state of the BusyBox submodule as a patch of the series.
#
# The workflow is: run busybox-prepare.sh, edit lib/busybox in place until it
# builds, then run this script to turn the edits into a patch. The submodule
# stays pinned to its release commit, every Tenok change lives in patches/.
#
#     ./scripts/busybox-refresh-patch.sh 0001-name-of-the-change
#
set -e

if [ $# -lt 1 ]; then
    echo "usage: $0 <patch-name-without-extension> [path]..."
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
BUSYBOX="${ROOT}/lib/busybox"
OUTPUT="${ROOT}/lib/busybox-patches/patches/$1.patch"
shift

# Only modifications to tracked files are captured. The build leaves generated
# headers and objects behind in the submodule and they must not end up in a
# patch.
git -C "${BUSYBOX}" diff -- "$@" >"${OUTPUT}"

if [ ! -s "${OUTPUT}" ]; then
    rm -f "${OUTPUT}"
    echo "lib/busybox has no change, nothing was written"
    exit 1
fi

echo "wrote ${OUTPUT} ($(grep -c '^+++' "${OUTPUT}") files)"
