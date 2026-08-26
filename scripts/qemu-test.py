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
    # Real BusyBox, linked into the firmware and reached through its own
    # multi-call dispatcher
    ("busybox", ["BusyBox v1.38.0", "multi-call binary",
     "Currently defined functions"], []),
    ("busybox echo hello world", ["hello world"], []),
    ("busybox cat " + POEM, ["Tell all the truth", "By Emily Dickinson"], []),
    ("busybox nosuch", ["applet not found"], []),
    # The BusyBox ash shell, entered from the Tenok shell. Everything between
    # "busybox sh" and "exit" is typed at ash's own prompt.
    ("busybox sh", ["built-in shell (ash)"], []),
    ("pwd", ["/"], []),
    ("cd /rom_data", [], ["can't cd"]),
    ("pwd", ["/rom_data"], []),
    ("cat poem.txt", ["Tell all the truth", "By Emily Dickinson"], []),
    ("cd more", [], ["can't cd"]),
    ("pwd", ["/rom_data/more"], []),
    ("cat animal.txt", ["The study of animals is called zoology"], []),
    ("echo the shell works", ["the shell works"], []),
    ("cd /", [], []),
    # The applets, reached from the shell without a fork
    ("ls /", ["dev", "rom_data"], []),
    ("ls -l /rom_data", ["-rw-r--r--", "261", "poem.txt"], []),
    ("wc /rom_data/poem.txt", ["11", "46", "261"], []),
    ("head -n 2 " + POEM, ["Tell all the truth", "Success in Circuit"],
     ["Too bright"]),
    ("uname -a", ["Tenok", "armv7m"], []),
    # Creating, renaming and removing, on the file system of Tenok
    ("mkdir /scratch", [], ["can't create"]),
    ("touch /scratch/alpha", [], ["cannot", "can't"]),
    ("ls /scratch", ["alpha"], []),
    ("mv /scratch/alpha /scratch/beta", [], ["can't rename"]),
    ("ls /scratch", ["beta"], ["alpha"]),
    ("rm /scratch/beta", [], ["can't remove"]),
    ("ls /scratch", [], ["beta"]),
    ("rm -r /scratch", [], ["can't remove"]),
    ("ls /", ["rom_data"], ["scratch"]),
    # An error message from the shell itself. It used to leave stderr pointing
    # at another descriptor, after which no prompt was ever printed again
    ("nosuchcmd", ["sh: nosuchcmd: not found"], []),
    ("echo after an error", ["after an error"], []),
    # The error string of a failed call, which reaches the message through the
    # "%m" of BusyBox, a conversion Tenok's printf does not know
    ("cd /nonexistent", ["No such file or directory"], [": m"]),
    # Standard input of an applet: the echo of the terminal, the copy made by
    # cat, and the end of file character that ends the read
    # How the echo of the terminal and the copy of cat interleave depends on
    # how the reads are chunked, what matters is that the input can be ended
    ("cat\rA\r\x04", ["A"], []),
    ("echo cat returned", ["cat returned"], []),
    ("wc\ra b c\r\x04", ["1", "3", "6"], []),
    # The interrupt character unwinds an applet the way a fatal error does
    ("cat\r\x03", ["^C"], []),
    ("echo $?", ["130"], []),
    # Redirection. Tenok has one file table for the whole system and its own
    # dup2() refuses the standard descriptors, so BusyBox keeps its own
    ("echo redirected > /out.txt", [], ["can't create"]),
    ("cat /out.txt", ["redirected"], []),
    ("echo appended >> /out.txt", [], []),
    ("wc -l /out.txt", ["2"], []),
    ("cat < /out.txt", ["redirected", "appended"], []),
    ("ls /nonexistent 2> /err.txt", [], ["No such file"]),
    ("cat /err.txt", ["No such file or directory"], []),
    ("rm /out.txt /err.txt", [], ["can't remove"]),
    # The shell language
    ("echo $((6 * 7))", ["42"], []),
    ("test -d /rom_data", [], []),
    ("echo $?", ["0"], []),
    ("[ -f /nonexistent ]", [], []),
    ("echo $?", ["1"], []),
    ("for i in a b c; do echo item $i; done", ["item a", "item b", "item c"], []),
    # Pattern matching of a case statement, which reaches the fnmatch() of the
    # compatibility layer through pmatch() of ash
    ("case abc in [a-z]?c) echo matched;; *) echo no;; esac", ["matched"], []),
    ("case Abc in [a-z]bc) echo yes;; *) echo not matched;; esac",
     ["not matched"], []),
    ("case abc in [!x-z]bc) echo negated;; *) echo no;; esac", ["negated"], []),
    # A mode Tenok can provide is granted, anything else is refused rather
    # than reported as a success that stores nothing
    ("mkdir -m 755 /m1", [], ["can't set permissions"]),
    ("mkdir -m 700 /m2", [], ["can't set permissions"]),
    ("stat -c %a /m1", ["755"], []),
    ("stat -c %a /m2", ["700"], []),
    ("chmod 644 /m2", [], ["chmod:"]),
    ("stat -c %a /m2", ["644"], []),
    # The permission bits are stored, so a mode asked for is the mode read
    # back. Tenok has one user and checks none of them, but what a program
    # writes it can read.
    ("stat -c %F /m2", ["directory"], []),
    ("ls -ld /m1", ["drwxr-xr-x"], []),
    # A file is created with the mode the caller asks for, less the bits the
    # umask withholds
    ("umask", ["0022"], []),
    ("echo hi > /u1", [], ["can't create"]),
    ("stat -c %a /u1", ["644"], []),
    ("ls -l /u1", ["-rw-r--r--"], []),
    ("umask 077", [], []),
    ("echo hi > /u2", [], ["can't create"]),
    ("stat -c %a /u2", ["600"], []),
    ("umask 022", [], []),
    ("rm /u1 /u2", [], ["can't remove"]),
    ("rm -r /m1 /m2", [], ["can't remove"]),
    # Device files. Reading one has to reach its end: /dev/null is empty and
    # /dev/rom is the image the file system was built from
    ("cat /dev/null", [], ["No such"]),
    ("wc /dev/null", ["0", "0", "0"], []),
    ("sha256sum /dev/rom", ["/dev/rom"], ["out of memory"]),
    # A file an applet has read has to remain removable: nothing may be left
    # open behind it
    ("cp /rom_data/poem.txt /k.txt", [], []),
    ("cmp /k.txt /rom_data/poem.txt", [], ["differ"]),
    ("md5sum /k.txt", ["ac3605abe32168761f9cec0c4dd182ec"], []),
    ("rm /k.txt", [], ["busy"]),
    # A failed call reports its reason: Tenok's fopen() has to set errno
    ("wc /nonexistent", ["No such file or directory"], []),
    ("dirname /", ["/"], []),
    ("dirname /a/b", ["/a"], []),
    # The read builtin polls its input, which reaches a redirected descriptor
    # only through the table BusyBox keeps for itself
    ("read line < /rom_data/poem.txt", [], []),
    ("echo first: $line", ["first: Tell all the truth"], []),
    ("exit", [], []),
    ("pwd", ["/"], []),
    # A second session has to find the globals of BusyBox as they were linked,
    # everything the first one allocated has been released by then
    ("busybox sh", ["built-in shell (ash)"], []),
    ("ls /", ["dev", "rom_data"], []),
    ("echo second session", ["second session"], []),
    ("exit", [], []),
    ("free", ["User heap"], []),
    # A shell script out of the read only file system. Running it needs no
    # fork, the shell of BusyBox is the one being started
    ("busybox sh /rom_data/demo.sh", ["6 passed, 0 failed"], ["FAIL"]),
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
