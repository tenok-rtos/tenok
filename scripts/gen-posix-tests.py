#!/usr/bin/env python3
"""
Work out which tests of the Open POSIX Test Suite Tenok can be measured
against, and write down what the build and the shell command need to know.

A test that names something Tenok does not have will not compile, and a test
that wants a second process cannot be run at all. Both are left out here rather
than in a list somebody has to keep, so that what is measured follows what the
system gains.
"""
import os
import re
import subprocess
import sys

SUITE = "lib/open-posix-testsuite"
INTERFACES = os.path.join(SUITE, "conformance", "interfaces")

# A test that asks for one of these wants what Tenok has no second process for
NEEDS_A_PROCESS = ("fork", "execl", "execv", "shm_open", "posix_spawn")


def defined_symbols(nm, paths):
    """Every symbol the given libraries or objects define."""
    found = set()

    for path in paths:
        if not os.path.exists(path):
            continue
        listing = subprocess.run([nm, "--defined-only", path],
                                 capture_output=True, text=True)
        for line in listing.stdout.split("\n"):
            fields = line.split()
            if len(fields) == 3 and fields[1] in "TWBDRV":
                found.add(fields[2])

    return found


def provided(cc, nm):
    """What a test is allowed to call: what the C library brings, and what
    Tenok itself defines. A name that is in neither is a name nothing will
    answer at the link.
    """
    listing = subprocess.run([cc, "-print-file-name=libc_nano.a"],
                             capture_output=True, text=True)
    libdir = os.path.dirname(listing.stdout.strip())
    libraries = [os.path.join(libdir, name) for name in
                 ("libc_nano.a", "libm.a", "libnosys.a", "libgcc.a")]

    objects = []
    for root in ("kernel", "user", "drivers", "platform", "tools"):
        for base, _, names in os.walk(root):
            objects += [os.path.join(base, name) for name in names
                        if name.endswith(".o")]

    return defined_symbols(nm, libraries + objects)


def undefined_symbols(nm, obj):
    listing = subprocess.run([nm, "--undefined-only", obj],
                             capture_output=True, text=True)
    return {line.split()[-1] for line in listing.stdout.split("\n")
            if line.strip()}


def usable(path, cc, nm, cflags, have):
    """Whether the test compiles and everything it calls will be answered."""
    obj = path + ".probe.o"
    result = subprocess.run(
        [cc, "-c", path, "-o", obj, "-Dtest_main=probe",
         "-Werror=implicit-function-declaration", "-w"] + cflags,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    if result.returncode != 0:
        return False

    try:
        return undefined_symbols(nm, obj) <= have
    finally:
        os.unlink(obj)


def symbol(interface, case):
    return ("opts_%s_%s" % (interface, case)).replace("-", "_").replace(".", "_")


def main():
    cc = os.environ.get("CC", "arm-none-eabi-gcc")
    nm = cc.replace("gcc", "nm")
    cflags = sys.argv[2:]
    have = provided(cc, nm)

    if not os.path.isdir(INTERFACES):
        sys.exit("%s is empty, run scripts/download-posix-tests.sh" % SUITE)

    found = []
    for interface in sorted(os.listdir(INTERFACES)):
        directory = os.path.join(INTERFACES, interface)
        if not os.path.isdir(directory):
            continue

        # Not an interface of POSIX but the frame the tests are written on
        if interface == "testfrmw":
            continue

        for name in sorted(os.listdir(directory)):
            if not name.endswith(".c"):
                continue

            # A test whose name says so is compiled and not run: it says only
            # that a header names what it should
            if name.endswith("-buildonly.c"):
                continue

            path = os.path.join(directory, name)
            with open(path, errors="replace") as source:
                text = source.read()

            if any(word in text for word in NEEDS_A_PROCESS):
                continue
            if not usable(path, cc, nm, cflags, have):
                continue

            found.append((interface, name[:-2], path))

    out = sys.argv[1]

    with open(out + ".h", "w") as header:
        header.write("""/**
 * @file
 *
 * The tests of the Open POSIX Test Suite that Tenok is measured against.
 * Written by scripts/gen-posix-tests.py from what the suite holds.
 */
#ifndef __OPTS_TESTS_H__
#define __OPTS_TESTS_H__

struct opts_test {
    const char *interface;
    const char *name;
    int (*run)(int argc, char **argv);
};

""")
        for interface, case, _ in found:
            header.write("int %s(int argc, char **argv);\n" %
                         symbol(interface, case))

        header.write("\nstatic const struct opts_test opts_tests[] = {\n")
        for interface, case, _ in found:
            header.write('    {"%s", "%s", %s},\n' %
                         (interface, case, symbol(interface, case)))
        header.write("};\n\n#endif\n")

    # What the makefile compiles, and the name each one is called by
    with open(out + ".mk", "w") as makefile:
        makefile.write("# Written by scripts/gen-posix-tests.py\n")
        makefile.write("OPTS_SRC :=\n")
        for interface, case, path in found:
            makefile.write("OPTS_SRC += $(PROJ_ROOT)/%s\n" % path)
            makefile.write("$(PROJ_ROOT)/%s: CFLAGS += -Dtest_main=%s\n" %
                           (path[:-2] + ".o", symbol(interface, case)))

    interfaces = len(set(interface for interface, _, _ in found))
    print("%d tests over %d interfaces" % (len(found), interfaces))


if __name__ == "__main__":
    main()
