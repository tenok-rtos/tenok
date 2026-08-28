#!/usr/bin/env bash
#
# Build mkdgiff, which renders a font into the glyph image format DirectFB2
# reads. It needs the DirectFB2 headers for the format it writes and FreeType
# for the rendering, and nothing else its own build would look for.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
tools=$root/lib/directfb-tools
dfb=$root/lib/directfb2

if [ ! -f "$tools/src/mkdgiff.c" ]; then
    echo "$0: lib/directfb-tools is empty, run 'git submodule update --init'" >&2
    exit 1
fi

# The DirectFB2 headers are written for a build that has already settled what
# a bool is, which this one has not
gcc -O2 -o "$tools/mkdgiff" "$tools/src/mkdgiff.c" \
    -include stdbool.h \
    -I "$dfb/include" -I "$dfb/lib" -I "$dfb" \
    $(pkg-config --cflags --libs freetype2) -lm
