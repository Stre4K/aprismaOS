#!/usr/bin/env python3
"""Write compile_commands.json for editors (clangd) from the real build.

Asks make for the commands it would run (make -n -B, nothing is built) in
every project, so each file gets exactly the flags it is built with:
architecture flags, CONFIG_* defines, include paths. The selected
architecture comes first; a second pass with the other one adds the files
only it builds (arch/i386 or arch/x86_64). The host unit tests are included
too. Include paths into the sysroot are pointed back at the source headers,
so "go to definition" opens the real file rather than the installed copy,
and GCC-only options clang does not know are left out.

aprisma.sh --gen-compile-db (or -d) runs this; it can also be run directly:
  scripts/gen_compile_db.py --arch x86_64
"""

import argparse
import json
import os
import shlex
import subprocess
import sys

ARCHES = {"x86_64": "x86_64-elf", "i386": "i686-elf"}
# GCC options clang (and so clangd) rejects; they only tune GCC's optimizer
GCC_ONLY = {"-fno-tree-loop-distribute-patterns"}
PROJECTS = ["libc", "user", "kernel"]  # the order aprisma.sh builds them in


def dry_run(directory, env, make, target):
    """The shell commands make would run in `directory`."""
    result = subprocess.run([make, "-n", "-B", *target], cwd=directory, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        sys.stderr.write(f"warning: make -n in {directory} failed:\n{result.stderr}")
    return result.stdout.splitlines()


def compile_entries(lines, directory, compilers, header_dirs):
    """Turn "<cc> ... -c file ..." lines into compile database entries."""
    entries = []
    for line in lines:
        try:
            words = shlex.split(line)
        except ValueError:
            continue  # e.g. an echo with odd quoting
        if not words or os.path.basename(words[0]) not in compilers or "-c" not in words:
            continue
        source = words[words.index("-c") + 1]
        arguments = []
        skip = False
        for i, word in enumerate(words):
            if skip:
                skip = False
                continue
            if word in ("-MD", "-MP") or word in GCC_ONLY:
                continue  # dependency files are a build detail
            # -I<sysroot>/include or "-I <sysroot>/include": the source headers instead
            if word == "-I" and i + 1 < len(words) and words[i + 1] in header_dirs:
                arguments += header_dirs[words[i + 1]]
                skip = True
                continue
            if word.startswith("-I") and word[2:] in header_dirs:
                arguments += header_dirs[word[2:]]
                continue
            arguments.append(word)
        entries.append({
            "directory": directory,
            "arguments": arguments,
            "file": os.path.normpath(os.path.join(directory, source)),
        })
    return entries


def arch_env(root, arch, base):
    host = ARCHES[arch]
    env = dict(base)
    env.update({
        "HOSTARCH": arch, "CC": f"{host}-gcc", "AS": f"{host}-as", "AR": f"{host}-ar",
        "CFLAGS": "-O2 -g", "CPPFLAGS": "",
        "ROOTDIR": root,
        "SYSROOT": os.path.join(root, "build/AprismaOS/sysroot"),
        "OBJ_DIR": os.path.join(root, "build/objects", arch),
    })
    return env


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--arch", choices=sorted(ARCHES), default="x86_64",
                        help="architecture whose flags shared files get (default: x86_64)")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--make", default=os.environ.get("MAKE", "make"))
    parser.add_argument("--output", help="default: <root>/compile_commands.json")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    sysroot_include = os.path.join(root, "build/AprismaOS/sysroot/include")
    source_headers = [f"-I{os.path.join(root, 'src/libc/include')}", f"-I{os.path.join(root, 'src/kernel/include')}"]
    # make prints the sysroot both normalized and as <project>/../../build/...
    header_dirs = {sysroot_include: source_headers}
    for project in PROJECTS:
        header_dirs[os.path.join(root, "src", project, "../../build/AprismaOS/sysroot/include")] = source_headers

    entries, seen = [], set()

    def add(new_entries):
        for entry in new_entries:
            if entry["file"] not in seen:  # the first flavour wins (libc before libk, chosen arch first)
                seen.add(entry["file"])
                entries.append(entry)

    others = [a for a in ARCHES if a != args.arch]
    for arch in [args.arch, *others]:
        env = arch_env(root, arch, os.environ)
        compilers = {env["CC"]}
        for project in PROJECTS:
            directory = os.path.join(root, "src", project)
            add(compile_entries(dry_run(directory, env, args.make, ["install"]), directory, compilers, header_dirs))

    # Host unit tests, built with the host compiler
    env = dict(os.environ)
    env.pop("CC", None)
    directory = os.path.join(root, "tests/unit")
    add(compile_entries(dry_run(directory, env, args.make, []), directory, {"cc", "gcc", "clang"}, header_dirs))

    output = args.output or os.path.join(root, "compile_commands.json")
    with open(output, "w") as f:
        json.dump(entries, f, indent=2)
        f.write("\n")
    print(f"{len(entries)} files in {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
