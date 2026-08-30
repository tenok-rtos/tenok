#!/usr/bin/env bash
#
# Fetch the Open POSIX Test Suite, which is what Tenok is measured against for
# conformance. It lives inside the Linux Test Project, and only the one
# directory of it is taken: the rest is written for a system with processes.
#
# It is fetched rather than kept as a submodule because it is needed only when
# the conformance tests are built, and because it is far larger than what it
# contributes to the firmware.
#
set -e

# The revision the tests are taken against, so that a run today and a run
# tomorrow measure the same thing
LTP_URL=https://github.com/linux-test-project/ltp.git
LTP_REV=d033df5fb4a3e4e7a9b510625f9c47236c76417b
SUBDIR=testcases/open_posix_testsuite

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST="${ROOT}/lib/open-posix-testsuite"

if [ -f "${DEST}/.tenok-revision" ] &&
   [ "$(cat "${DEST}/.tenok-revision")" = "${LTP_REV}" ]; then
    echo "already at ${LTP_REV:0:10}"
    exit 0
fi

# A directory of its own, so that what the caller is standing in is never the
# one that gets cloned over and removed
scratch=$(mktemp -d)
trap 'rm -rf "${scratch}"' EXIT

echo "fetch  ${LTP_REV:0:10}"
git -C "${scratch}" init -q ltp
git -C "${scratch}/ltp" remote add origin "${LTP_URL}"
git -C "${scratch}/ltp" config core.sparseCheckout true
echo "${SUBDIR}/*" > "${scratch}/ltp/.git/info/sparse-checkout"
git -C "${scratch}/ltp" fetch -q --depth 1 origin "${LTP_REV}"
git -C "${scratch}/ltp" checkout -q FETCH_HEAD

rm -rf "${DEST}"
mkdir -p "$(dirname "${DEST}")"
mv "${scratch}/ltp/${SUBDIR}" "${DEST}"
echo "${LTP_REV}" > "${DEST}/.tenok-revision"

echo "wrote  lib/open-posix-testsuite ($(find "${DEST}" -name '*.c' | wc -l) sources)"
