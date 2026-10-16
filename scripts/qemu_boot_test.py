#!/usr/bin/env python3
"""Boot an AprismaOS kernel or ISO in QEMU and check what it prints.

The kernel only writes to the VGA text screen, so the test reads the screen
straight out of guest memory (physical 0xB8000) through the QEMU monitor.
It waits for the boot to finish, optionally types shell commands and waits
for their output, and fails on a kernel panic, an unhandled exception, QEMU
exiting (e.g. a triple fault, since -no-reboot is used) or a timeout. The
kernel's serial output (COM1) is recorded too and printed on failure.

Examples:
  scripts/qemu_boot_test.py --arch i386 --kernel build/AprismaOS/sysroot/boot/aprisma.kernel
  scripts/qemu_boot_test.py --arch x86_64 --iso build/aprisma.iso \\
      --run "test heap" "heap test: .*passed"
"""

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

VGA_TEXT_ADDR = 0xB8000
VGA_COLS, VGA_ROWS = 80, 25

BOOT_DONE = r"\[KEYBOARD\] Initialized successfully"
FAILURE_PATTERNS = [r"KERNEL PANIC", r"Unhandled CPU exception", r"Unhandled interrupt"]

# QEMU sendkey names for characters that are not plain letters or digits
KEY_NAMES = {" ": "spc", "-": "minus", ".": "dot", "/": "slash", "=": "equal", ",": "comma"}


class TestFailure(Exception):
    pass


class Monitor:
    """Minimal client for the QEMU human monitor over a unix socket."""

    PROMPT = b"(qemu) "

    def __init__(self, path, timeout):
        deadline = time.monotonic() + timeout
        while True:
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(path)
                break
            except OSError:
                self.sock.close()
                if time.monotonic() > deadline:
                    raise TestFailure("could not connect to the QEMU monitor")
                time.sleep(0.1)
        self.sock.settimeout(10)
        self._read_until_prompt()

    def _read_until_prompt(self):
        data = b""
        while not data.endswith(self.PROMPT):
            chunk = self.sock.recv(4096)
            if not chunk:
                raise TestFailure("QEMU monitor closed the connection")
            data += chunk
        return data

    def command(self, line):
        self.sock.sendall(line.encode() + b"\n")
        return self._read_until_prompt()

    def close(self):
        self.sock.close()


def read_screen(monitor, dump_path):
    """Return the VGA text screen as 25 lines of text."""
    # QEMU runs inside the temp directory, so pass a relative file name: the
    # monitor parses a leading '/' after the size as a division
    monitor.command(f"pmemsave {VGA_TEXT_ADDR:#x} {VGA_COLS * VGA_ROWS * 2} {os.path.basename(dump_path)}")
    with open(dump_path, "rb") as f:
        data = f.read()
    chars = data[0::2]  # every cell is a character byte followed by a colour byte
    lines = []
    for row in range(VGA_ROWS):
        line = chars[row * VGA_COLS:(row + 1) * VGA_COLS]
        lines.append("".join(chr(c) if 32 <= c < 127 else " " for c in line).rstrip())
    return "\n".join(lines)


def find_initrd(kernel, explicit, disabled):
    """The ramdisk for a -kernel boot: --initrd, or initrd.tar next to the kernel."""
    if disabled:
        return None
    if explicit:
        return os.path.abspath(explicit)
    candidate = os.path.join(os.path.dirname(os.path.abspath(kernel)), "initrd.tar")
    return candidate if os.path.exists(candidate) else None


def read_serial_log(path):
    """The kernel's serial output so far, with \\r\\n turned into \\n."""
    try:
        with open(path, "rb") as f:
            return f.read().decode(errors="replace").replace("\r\n", "\n")
    except FileNotFoundError:
        return ""


def type_keys(monitor, text):
    for ch in text:
        if ch.isalnum():
            key = ch.lower() if not ch.isupper() else "shift-" + ch.lower()
        elif ch in KEY_NAMES:
            key = KEY_NAMES[ch]
        else:
            raise TestFailure(f"don't know how to type {ch!r}")
        monitor.command(f"sendkey {key}")
        time.sleep(0.02)


def command_typed(screen, command):
    """True if the line being typed (the last one on screen) is `command`,
    alone or after a prompt."""
    lines = screen.rstrip().split("\n")
    line = lines[-1] if lines else ""
    return line == command or line.endswith(" " + command)


def type_command(monitor, dump_path, command, attempts=3):
    """Type a command line and press Enter.

    QEMU's PS/2 keyboard buffers only a few key presses, so when the host
    is busy and the guest falls behind, keys are dropped. Wait until the
    whole command shows up on screen (the guest has read every key) before
    pressing Enter; if keys went missing, cancel the line with Ctrl+C and
    type it again."""
    for _ in range(attempts):
        type_keys(monitor, command)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if command_typed(read_screen(monitor, dump_path), command):
                monitor.command("sendkey ret")
                return
            time.sleep(0.1)
        print(f"note: key presses lost while typing '{command}', typing it again", file=sys.stderr)
        monitor.command("sendkey ctrl-c")
        time.sleep(0.5)
    raise TestFailure(f"could not type '{command}': key presses were lost")


def wait_for(monitor, qemu, dump_path, pattern, timeout, skip_grub_menu=True):
    """Poll the screen until `pattern` shows up; fail on errors or timeout."""
    deadline = time.monotonic() + timeout
    grub_handled = False
    screen = ""

    while time.monotonic() < deadline:
        if qemu.poll() is not None:
            raise TestFailure(f"QEMU exited with code {qemu.returncode} (triple fault?)\n{screen}")

        screen = read_screen(monitor, dump_path)

        for failure in FAILURE_PATTERNS:
            if re.search(failure, screen):
                raise TestFailure(f"kernel reported a failure ({failure})\n{screen}")

        if re.search(pattern, screen):
            return screen

        # The ISO's GRUB menu waits for a key press
        if skip_grub_menu and not grub_handled and "GNU GRUB" in screen:
            monitor.command("sendkey ret")
            grub_handled = True

        time.sleep(0.25)

    raise TestFailure(f"timed out after {timeout}s waiting for /{pattern}/\n{screen}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--arch", choices=["i386", "x86_64"], required=True)
    image = parser.add_mutually_exclusive_group(required=True)
    image.add_argument("--kernel", help="boot this kernel ELF with QEMU's Multiboot loader")
    image.add_argument("--iso", help="boot this ISO image (GRUB)")
    parser.add_argument("--run", nargs=2, action="append", default=[], metavar=("COMMAND", "EXPECT"),
                        help="after boot, type COMMAND and wait for the regex EXPECT (repeatable)")
    parser.add_argument("--timeout", type=float, default=60, help="seconds allowed for each step")
    parser.add_argument("--boot-done", default=BOOT_DONE, metavar="REGEX",
                        help="screen text that marks the end of the boot (default: keyboard initialized)")
    parser.add_argument("--initrd", help="ramdisk to load with --kernel (default: initrd.tar next to the kernel)")
    parser.add_argument("--no-initrd", action="store_true", help="boot --kernel without a ramdisk")
    parser.add_argument("--no-serial", action="store_true",
                        help="boot without a serial port (-serial none)")
    parser.add_argument("--check-serial", action="store_true",
                        help="also require the boot marker and every --run result in the serial log")
    parser.add_argument("--qemu-arg", action="append", default=[], metavar="ARG",
                        help="extra QEMU argument, e.g. --qemu-arg=-machine --qemu-arg=pc,i8042=off (repeatable)")
    parser.add_argument("--objcopy", default="objcopy", help="objcopy used to convert x86_64 kernels")
    args = parser.parse_args()

    qemu_bin = f"qemu-system-{args.arch}"
    if shutil.which(qemu_bin) is None:
        print(f"error: {qemu_bin} not found", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="aprisma-boot-") as tmp:
        monitor_path = os.path.join(tmp, "monitor.sock")
        dump_path = os.path.join(tmp, "vga.bin")
        serial_path = os.path.join(tmp, "serial.log")
        serial_args = ["-serial", "none"] if args.no_serial else ["-serial", f"file:{serial_path}"]

        # QEMU runs inside tmp, so image paths must be absolute
        if args.kernel:
            kernel = os.path.abspath(args.kernel)
            if args.arch == "x86_64":
                # QEMU's built-in Multiboot loader only accepts 32-bit ELF files
                kernel = os.path.join(tmp, "aprisma32.kernel")
                subprocess.run([args.objcopy, "-O", "elf32-i386", os.path.abspath(args.kernel), kernel], check=True)
            boot_args = ["-kernel", kernel]
            initrd = find_initrd(args.kernel, args.initrd, args.no_initrd)
            if initrd:
                boot_args += ["-initrd", initrd]
        else:
            boot_args = ["-cdrom", os.path.abspath(args.iso), "-boot", "d"]

        qemu = subprocess.Popen(
            [qemu_bin, *boot_args, "-m", "128M", "-display", "none", "-no-reboot",
             "-monitor", f"unix:{monitor_path},server,nowait", *serial_args, *args.qemu_arg],
            cwd=tmp, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

        monitor = None
        try:
            monitor = Monitor(monitor_path, timeout=10)

            wait_for(monitor, qemu, dump_path, args.boot_done, args.timeout)
            print(f"ok: {args.arch} {'kernel' if args.kernel else 'ISO'} booted")

            for command, expect in args.run:
                type_command(monitor, dump_path, command)
                wait_for(monitor, qemu, dump_path, expect, args.timeout, skip_grub_menu=False)
                print(f"ok: '{command}' printed /{expect}/")

            if args.check_serial:
                time.sleep(0.5)  # let QEMU flush the serial file
                log = read_serial_log(serial_path)
                for pattern in [args.boot_done, *(expect for _, expect in args.run)]:
                    if not re.search(pattern, log):
                        raise TestFailure(f"serial log is missing /{pattern}/")
                print("ok: serial log has the same output")

            print(read_screen(monitor, dump_path))
            return 0
        except TestFailure as e:
            print(f"FAIL: {e}", file=sys.stderr)
            log = read_serial_log(serial_path)
            if log:
                tail = "\n".join(log.splitlines()[-60:])
                print(f"--- serial log (last 60 lines) ---\n{tail}", file=sys.stderr)
            if qemu.poll() is not None and qemu.stderr:
                sys.stderr.write(qemu.stderr.read().decode(errors="replace"))
            return 1
        finally:
            if monitor:
                monitor.close()
            if qemu.poll() is None:
                qemu.kill()
                qemu.wait()


if __name__ == "__main__":
    sys.exit(main())
