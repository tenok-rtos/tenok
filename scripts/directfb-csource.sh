#!/usr/bin/env bash
#
# Build directfb-csource, which turns the data files of the DirectFB2 examples
# into C. Without the DirectFB and PNG options its own build would look for, it
# is a single C file that reads the already converted .dfiff and .dgiff and
# needs nothing but the standard headers.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
csource=$root/lib/directfb-csource

if [ ! -f "$csource/src/directfb-csource.c" ]; then
    echo "$0: lib/directfb-csource is empty, run 'git submodule update --init'" >&2
    exit 1
fi

gcc -O2 -o "$csource/directfb-csource" "$csource/src/directfb-csource.c"
