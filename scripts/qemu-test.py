#!/usr/bin/env python3
"""Smoke test of the Tenok shell under QEMU.

The script boots the firmware with `qemu-system-arm`, drives the console over
a pseudo terminal and checks the output of a list of commands.

Usage: ./scripts/qemu-test.py [path/to/tenok.elf]
"""

import os
import pty
import re
import select
import subprocess
import sys
import time

QEMU = ["qemu-system-arm", "-nographic", "-cpu", "cortex-m4",
        "-M", "netduinoplus2", "-serial", "pty", "-serial", "null",
        "-serial", "null", "-kernel"]

# Escape sequences emitted by the line editor of the shell
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|\x1b[=>c]")

POEM = "/rom_data/poem.txt"

# (command, [expected substrings], [forbidden substrings])
TESTS = [
    ("help", ["supported commands"], []),
    ("ls /", ["dev/", "rom_data/"], []),
    ("cat " + POEM, ["Tell all the truth", "By Emily Dickinson"], []),
    ("cd /rom_data", [], ["No such file"]),
    ("pwd", ["/rom_data"], []),
    ("ls", ["poem.txt", "more/"], []),
    ("cd /", [], ["No such file"]),
    ("file /dev/console", ["character device"], []),
    ("echo hello world", ["hello world"], []),
    ("uname", ["Tenok", "armv7m"], []),
    ("ps", ["shell", "filesysd"], []),
    ("free", ["Page memory", "User heap"], []),
    ("uptime", ["seconds up"], []),
]


def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else "tenok.elf"

    if not os.path.exists(elf):
        print("%s not found, run `make` first" % elf)
        return 1

    master, slave = pty.openpty()
    qemu = subprocess.Popen(QEMU + [elf], stdin=slave, stdout=slave,
                            stderr=subprocess.STDOUT, close_fds=True)
    os.close(slave)

    # QEMU reports the pseudo terminal of the console on its own output
    buf, console, deadline = b"", None, time.time() + 15
    while time.time() < deadline and console is None:
        if select.select([master], [], [], 0.5)[0]:
            buf += os.read(master, 4096)
            found = re.search(rb"char device redirected to (\S+)", buf)
            if found:
                console = found.group(1).decode()

    if not console:
        print("failed to attach to the console of the guest")
        qemu.kill()
        return 1

    fd = os.open(console, os.O_RDWR | os.O_NOCTTY)

    def read_until_prompt(timeout=25.0, idle=0.6):
        out, last, end = b"", time.time(), time.time() + timeout
        while time.time() < end:
            if select.select([fd], [], [], 0.1)[0]:
                try:
                    data = os.read(fd, 4096)
                except OSError:
                    break
                if data:
                    out += data
                    last = time.time()
                    continue
            if (time.time() - last) >= idle:
                text = ANSI.sub("", out.decode(errors="replace"))
                if text.endswith("$ ") or text.endswith("# "):
                    break
        return ANSI.sub("", out.decode(errors="replace"))

    def run(command):
        # The console has no flow control, feed it one character at a time
        for char in command.encode():
            os.write(fd, bytes([char]))
            time.sleep(0.005)
        os.write(fd, b"\r")

        raw = read_until_prompt()

        # The line editor echoes every keystroke, the output of the command
        # only starts after the first line break
        start = raw.find("\r\n")
        output = raw[start + 2:] if start >= 0 else raw

        return (re.sub(r"[^\r\n]*[$#] ?$", "", output).replace("\r\n", "\n"),
                raw)

    read_until_prompt()

    failed = 0

    for command, expected, forbidden in TESTS:
        output, raw = run(command)
        errors = [s for s in expected if s not in output]
        errors += ["(unexpected) " + s for s in forbidden if s in output]

        if errors:
            failed += 1
            print("FAIL  %s" % command)
            for error in errors:
                print("      missing: %s" % error)
            print("      output: %r" % output)
            # The console has no flow control, so a failure can be a character
            # the target never received. The echo is in here to say so
            print("      echoed: %r" % raw)
        else:
            print("ok    %s" % command)

    qemu.kill()
    qemu.wait()

    print("\n%d/%d tests passed" % (len(TESTS) - failed, len(TESTS)))

    return 1 if failed else 0


sys.exit(main())
