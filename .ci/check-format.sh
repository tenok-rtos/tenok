!/usr/bin/env bash

ROOT=$(git rev-parse --show-toplevel)

FORMAT=".*\.\(c\|h\)"
EXCLUDE="-path ${ROOT}/lib -o -path ${ROOT}/platform"

SOURCES=$(find "${ROOT}" -type d \( ${EXCLUDE} \) -prune -o -type f -regex ${FORMAT})

set -x

for file in ${SOURCES};
do
    clang-format-12 ${file} > expected-format
    diff -u -p --label="${file}" --label="expected coding style" ${file} expected-format
done

exit $(clang-format-12 --output-replacements-xml ${SOURCES} | egrep -c "</replacement>")
