#!/usr/bin/env bash
#
# Build fluxcomp, which turns the interface descriptions of DirectFB2 into C.
# It is a single C++ file that wants a config.h naming its own version, which
# its autotools build would have written.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
flux=$root/lib/flux

if [ ! -f "$flux/src/fluxcomp.cpp" ]; then
    echo "$0: lib/flux is empty, run 'git submodule update --init'" >&2
    exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

version=$(sed -n 's/^FLUXCOMP_\(MAJOR\|MINOR\|MICRO\)_VERSION=\(.*\)/\2/p' \
          "$flux/configure.in" | paste -sd.)

{
    echo "#pragma once"
    echo "#define PACKAGE \"flux\""
    echo "#define VERSION \"$version\""
    echo "#define FLUXCOMP_VERSION \"$version\""
    echo "#define FLUXCOMP_MAJOR_VERSION ${version%%.*}"
    echo "#define FLUXCOMP_MINOR_VERSION $(echo "$version" | cut -d. -f2)"
    echo "#define FLUXCOMP_MICRO_VERSION ${version##*.}"
} > "$tmp/config.h"

g++ -O2 -I "$tmp" -o "$flux/fluxcomp" "$flux/src/fluxcomp.cpp"
