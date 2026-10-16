#!/usr/bin/env python3
"""Keyboard and command-line tests for AprismaOS in QEMU.

Each scenario boots a fresh VM, sends key presses and releases through
QMP (input-send-event, so keys can be held across other keys) and checks
the VGA text screen, read the same way as qemu_boot_test.py.

Example:
  scripts/qemu_keyboard_test.py --arch x86_64 --kernel build/AprismaOS/sysroot/boot/aprisma.kernel
"""

import argparse
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu_boot_test import BOOT_DONE, Monitor, TestFailure, find_initrd, read_screen, wait_for  # noqa: E402

KEY_DELAY = 0.03  # seconds between key events

# QMP qcodes for characters that are not plain lowercase letters or digits
QCODES = {" ": "spc", "-": "minus", ".": "dot", "/": "slash", "=": "equal", ",": "comma", "'": "apostrophe"}


class Qmp:
    """Minimal QMP client: just enough to send key events."""

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
                    raise TestFailure("could not connect to QMP")
                time.sleep(0.1)
        self.sock.settimeout(10)
        self.file = self.sock.makefile("rw")
        self.file.readline()  # greeting
        self.execute("qmp_capabilities")

    def execute(self, command, **arguments):
        message = {"execute": command}
        if arguments:
            message["arguments"] = arguments
        self.file.write(json.dumps(message) + "\n")
        self.file.flush()
        while True:  # skip asynchronous events until the reply arrives
            reply = json.loads(self.file.readline())
            if "return" in reply:
                return reply["return"]
            if "error" in reply:
                raise TestFailure(f"QMP {command} failed: {reply['error']}")

    def key(self, qcode, down):
        event = {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}
        self.execute("input-send-event", events=[event])
        time.sleep(KEY_DELAY)

    def down(self, qcode):
        self.key(qcode, True)

    def up(self, qcode):
        self.key(qcode, False)

    def tap(self, *qcodes):
        """Press the keys in order, then release them in reverse (a chord)."""
        for q in qcodes:
            self.down(q)
        for q in reversed(qcodes):
            self.up(q)

    def type(self, text):
        for ch in text:
            if ch == "\n":
                self.tap("ret")
            elif ch.isupper():
                self.tap("shift", ch.lower())
            elif ch.isalnum():
                self.tap(ch)
            elif ch in QCODES:
                self.tap(QCODES[ch])
            else:
                raise TestFailure(f"don't know how to type {ch!r}")


class Vm:
    """A booted VM with a monitor (screen) and QMP (keys) connection."""

    def __init__(self, args, tmp):
        self.tmp = tmp
        self.dump = os.path.join(tmp, "vga.bin")
        monitor_path = os.path.join(tmp, "monitor.sock")
        qmp_path = os.path.join(tmp, "qmp.sock")
        for path in (monitor_path, qmp_path):
            if os.path.exists(path):
                os.unlink(path)

        self.qemu = subprocess.Popen(
            [f"qemu-system-{args.arch}", "-kernel", args.boot_kernel, *args.initrd_args, "-m", "128M",
             "-display", "none", "-no-reboot",
             "-monitor", f"unix:{monitor_path},server,nowait",
             "-qmp", f"unix:{qmp_path},server,nowait"],
            cwd=tmp, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.monitor = Monitor(monitor_path, timeout=10)
        self.qmp = Qmp(qmp_path, timeout=10)
        self.timeout = args.timeout
        self.wait(BOOT_DONE)

    def screen(self):
        return read_screen(self.monitor, self.dump)

    def lines(self):
        return self.screen().split("\n")

    def wait(self, pattern):
        return wait_for(self.monitor, self.qemu, self.dump, pattern, self.timeout, skip_grub_menu=False)

    def settle(self):
        time.sleep(0.5)  # let the kernel process the last keys

    def close(self):
        self.monitor.close()
        if self.qemu.poll() is None:
            self.qemu.kill()
            self.qemu.wait()


def expect(condition, message, vm):
    if not condition:
        raise TestFailure(f"{message}\n{vm.screen()}")


# ---------------------------------------------------------------- scenarios

def typing_runs_a_command(vm):
    vm.qmp.type("echo Hello world\n")
    vm.wait(r"(?m)^Hello world$")


def ctrl_c_discards_the_line(vm):
    vm.qmp.type("echo bad")
    vm.qmp.tap("left")
    vm.qmp.tap("left")  # Ctrl+C with the cursor mid-line
    vm.qmp.tap("ctrl", "c")
    vm.qmp.type("echo good\n")
    vm.wait(r"(?m)^good$")
    expect(re.search(r"(?m)^echo bad\^C$", vm.screen()), "^C must follow the whole line", vm)
    expect(not re.search(r"(?m)^bad", vm.screen()), "the cancelled line must not run", vm)


def right_ctrl_works_like_left(vm):
    vm.qmp.type("echo bad")
    vm.qmp.tap("ctrl_r", "c")
    vm.qmp.type("echo good\n")
    vm.wait(r"(?m)^good$")
    expect(re.search(r"(?m)^echo bad\^C$", vm.screen()), "Right Ctrl+C must cancel the line", vm)


def shift_stays_down_while_the_other_is_released(vm):
    vm.qmp.type("echo ")
    vm.qmp.down("shift")
    vm.qmp.tap("shift_r")  # press and release the other Shift
    vm.qmp.tap("a")
    vm.qmp.up("shift")
    vm.qmp.type("\n")
    vm.wait(r"(?m)^A$")


def home_end_delete_edit_the_line(vm):
    vm.qmp.type("cho hi")
    vm.qmp.tap("home")
    vm.qmp.type("e")
    vm.qmp.tap("end")
    vm.qmp.type("x\n")
    vm.wait(r"(?m)^hix$")

    vm.qmp.type("echoo yes")
    vm.qmp.tap("home")
    for _ in range(4):
        vm.qmp.tap("right")
    vm.qmp.tap("delete")
    vm.qmp.type("\n")
    vm.wait(r"(?m)^yes$")


def unknown_keys_are_ignored(vm):
    vm.qmp.type("echo kp")
    for key in ("insert", "alt_r", "meta_l", "f5"):
        vm.qmp.tap(key)
    vm.qmp.tap("kp_enter")  # keypad Enter runs the line
    vm.wait(r"(?m)^kp$")
    expect("UNKNOWN" not in vm.screen(), "unknown keys must not print anything", vm)


def tab_and_escape_are_not_inserted(vm):
    vm.qmp.type("echo a")
    vm.qmp.tap("tab")
    vm.qmp.tap("esc")
    vm.qmp.type("b\n")
    vm.wait(r"(?m)^ab$")


def clear_moves_the_cursor_to_the_top(vm):
    vm.qmp.type("clear\n")
    vm.settle()
    vm.qmp.type("echo top\n")
    vm.wait(r"(?m)^top$")
    lines = vm.lines()
    expect(lines[0] == "echo top" and lines[1] == "top", "after clear, input must start at the top", vm)


def ctrl_l_clears_and_keeps_the_line(vm):
    vm.qmp.type("echo keep")
    vm.qmp.tap("ctrl", "l")
    vm.settle()
    lines = vm.lines()
    expect(lines[0] == "echo keep" and not any(lines[1:]), "Ctrl+L must clear and redraw the line at the top", vm)
    vm.qmp.type("\n")
    vm.wait(r"(?m)^keep$")


# ---------------------------------------------------- user programs (read)

def usermode(scenario):
    """Mark a scenario that needs user programs (x86_64 only so far)."""
    scenario.usermode = True
    return scenario


@usermode
def read_returns_a_line_typed_after_the_prompt(vm):
    vm.qmp.type("run greet\n")
    vm.wait(r"What's your name\?")  # screen lines lose trailing spaces
    vm.qmp.type("Ada\n")
    vm.wait(r"\[\d+ greet\] exited with code 0")
    expect(re.search(r"(?m)^What's your name\? Ada$", vm.screen()), "input must follow the prompt", vm)
    expect(re.search(r"(?m)^Hello, Ada!$", vm.screen()), "the program must get the line", vm)


@usermode
def read_supports_line_editing(vm):
    vm.qmp.type("run greet\n")
    vm.wait(r"What's your name\?")  # screen lines lose trailing spaces
    vm.qmp.type("Adx")
    vm.qmp.tap("backspace")
    vm.qmp.type("a")
    vm.qmp.tap("home")
    vm.qmp.tap("end")
    vm.qmp.type("\n")
    vm.wait(r"(?m)^Hello, Ada!$")


@usermode
def read_gets_each_line_and_ctrl_d_ends_input(vm):
    vm.qmp.type("run upper\n")
    vm.settle()
    vm.qmp.type("abc\n")
    vm.wait(r"(?m)^ABC$")
    vm.qmp.type("Hello World\n")
    vm.wait(r"(?m)^HELLO WORLD$")
    vm.qmp.tap("ctrl", "d")
    vm.wait(r"\[\d+ upper\] exited with code 2")


@usermode
def ctrl_c_kills_a_busy_program(vm):
    vm.qmp.type("run forever\n")
    vm.settle()
    vm.qmp.tap("ctrl", "c")
    vm.wait(r"\[\d+ forever\] killed: interrupted \(Ctrl\+C\)")
    vm.wait(r"\[\d+ forever\] was killed")
    vm.qmp.type("echo alive\n")
    vm.wait(r"(?m)^alive$")


@usermode
def ctrl_c_kills_a_program_waiting_for_input(vm):
    vm.qmp.type("run greet\n")
    vm.wait(r"What's your name\?")  # screen lines lose trailing spaces
    vm.qmp.tap("ctrl", "c")
    vm.wait(r"\[\d+ greet\] was killed")
    vm.qmp.type("echo alive\n")
    vm.wait(r"(?m)^alive$")


@usermode
def input_typed_ahead_reaches_the_program(vm):
    vm.qmp.type("run greet\nBob\n")  # the name is typed before the prompt appears
    vm.wait(r"(?m)^Hello, Bob!$")


@usermode
def cat_without_arguments_copies_keyboard_input(vm):
    vm.qmp.type("run cat\n")
    vm.settle()
    vm.qmp.type("first line\n")
    vm.wait(r"(?m)^first line\n+first line$")  # typed, then printed back
    vm.qmp.tap("ctrl", "d")
    vm.wait(r"\[\d+ cat\] exited with code 0")


def ctrl_d_at_the_shell_prompt_is_harmless(vm):
    vm.qmp.tap("ctrl", "d")
    vm.qmp.type("echo still here\n")
    vm.wait(r"(?m)^still here$")


SCENARIOS = [
    typing_runs_a_command,
    ctrl_c_discards_the_line,
    right_ctrl_works_like_left,
    shift_stays_down_while_the_other_is_released,
    home_end_delete_edit_the_line,
    unknown_keys_are_ignored,
    tab_and_escape_are_not_inserted,
    clear_moves_the_cursor_to_the_top,
    ctrl_l_clears_and_keeps_the_line,
    ctrl_d_at_the_shell_prompt_is_harmless,
    read_returns_a_line_typed_after_the_prompt,
    read_supports_line_editing,
    read_gets_each_line_and_ctrl_d_ends_input,
    ctrl_c_kills_a_busy_program,
    ctrl_c_kills_a_program_waiting_for_input,
    input_typed_ahead_reaches_the_program,
    cat_without_arguments_copies_keyboard_input,
]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--arch", choices=["i386", "x86_64"], required=True)
    parser.add_argument("--kernel", required=True, help="kernel ELF to boot with QEMU's Multiboot loader")
    parser.add_argument("--timeout", type=float, default=30, help="seconds allowed for each wait")
    parser.add_argument("--objcopy", default="objcopy", help="objcopy used to convert x86_64 kernels")
    parser.add_argument("filter", nargs="?", help="run only scenarios whose name contains this")
    args = parser.parse_args()

    if shutil.which(f"qemu-system-{args.arch}") is None:
        print(f"error: qemu-system-{args.arch} not found", file=sys.stderr)
        return 2

    failed = 0
    with tempfile.TemporaryDirectory(prefix="aprisma-kbd-") as tmp:
        initrd = find_initrd(args.kernel, None, False)
        args.initrd_args = ["-initrd", initrd] if initrd else []
        args.boot_kernel = os.path.abspath(args.kernel)
        if args.arch == "x86_64":
            # QEMU's built-in Multiboot loader only accepts 32-bit ELF files
            args.boot_kernel = os.path.join(tmp, "aprisma32.kernel")
            subprocess.run([args.objcopy, "-O", "elf32-i386", os.path.abspath(args.kernel), args.boot_kernel],
                           check=True)

        for scenario in SCENARIOS:
            if args.filter and args.filter not in scenario.__name__:
                continue
            if getattr(scenario, "usermode", False) and args.arch != "x86_64":
                print(f"skip: {scenario.__name__} (no user mode on {args.arch})")
                continue
            vm = None
            try:
                vm = Vm(args, tmp)
                scenario(vm)
                print(f"ok: {scenario.__name__}")
            except TestFailure as e:
                failed += 1
                print(f"FAIL: {scenario.__name__}: {e}")
            finally:
                if vm:
                    vm.close()

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
