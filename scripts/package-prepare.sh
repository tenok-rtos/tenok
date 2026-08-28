#!/usr/bin/env bash
#
# Reset the submodule of a package to its pinned commit and apply the Tenok
# patch series on top of it. Running it repeatedly is safe and always produces
# the same tree, so the submodule itself is never modified in git.
#
# The patches live in a repository of their own, because a patch carries the
# surrounding lines of the file it changes and is covered by the licence of the
# package it is against.
#
set -e

package="$1"

if [ -z "${package}" ]; then
    echo "usage: $0 <package>" >&2
    exit 1
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SOURCE="${ROOT}/lib/${package}"
SERIES="${ROOT}/lib/package-patches/${package}/patches"

if [ ! -e "${SOURCE}/.git" ] || [ ! -d "${SERIES}" ]; then
    echo "The submodules are empty. Run:"
    echo "    git submodule update --init lib/${package} lib/package-patches"
    exit 1
fi

echo "reset  ${package} $(git -C "${SOURCE}" describe --tags --always)"
git -C "${SOURCE}" checkout -q .
git -C "${SOURCE}" clean -qfdx

shopt -s nullglob
patches=("${SERIES}"/*.patch)

if [ ${#patches[@]} -eq 0 ]; then
    echo "no patch to apply"
    exit 0
fi

for patch in "${patches[@]}"; do
    echo "apply  $(basename "${patch}")"
    git -C "${SOURCE}" apply --whitespace=nowarn "${patch}"
done
