#!/usr/bin/env bash
#
# Reset the BusyBox submodule to its pinned commit and apply the Tenok patch
# series on top of it. Running it repeatedly is safe and always produces the
# same tree, so the submodule itself is never modified in git.
#
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUSYBOX="${ROOT}/lib/busybox"
SERIES="${ROOT}/lib/busybox-patches/patches"

if [ ! -f "${BUSYBOX}/Makefile" ] || [ ! -d "${SERIES}" ]; then
    echo "The submodules are empty. Run:"
    echo "    git submodule update --init lib/busybox lib/busybox-patches"
    exit 1
fi

echo "reset  $(git -C "${BUSYBOX}" describe --tags --always)"
git -C "${BUSYBOX}" checkout -q .
git -C "${BUSYBOX}" clean -qfdx

shopt -s nullglob
patches=("${SERIES}"/*.patch)

if [ ${#patches[@]} -eq 0 ]; then
    echo "no patch to apply"
    exit 0
fi

for patch in "${patches[@]}"; do
    echo "apply  $(basename "${patch}")"
    git -C "${BUSYBOX}" apply --whitespace=nowarn "${patch}"
done
