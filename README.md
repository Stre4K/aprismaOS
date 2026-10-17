![AprismaOS Logo](docs/aprismaOS-wide.png "AprismaOS Logo")


# **Welcome to AprismaOS!**


Welcome to **AprismaOS**, my grand project to build my own operating system, **Aprisma**!
This project is a **work in progress** and subject to major changes.

AprismaOS is primarily a **learning project** focused on OS kernel development and
gaining experience with lower-level programming. It builds for **x86_64** (64-bit long mode)
and **i686** (32-bit protected mode) from the same source tree. The 64-bit kernel is the
main target: user programs only run there so far.

---

# **Table of Contents**
- [Features](#features)
- [Using AprismaOS](#using-aprismaos)
- [AprismaOS Development Goals](#aprismaos-development-goals)
  - [Current Focus](#current-focus)
  - [Beginner-Friendly Goals](#beginner-friendly-goals)
  - [Intermediate Goals](#intermediate-goals)
  - [Advanced Goals](#advanced-goals)
- [Building and Running](#building-and-running-aprismaos)
  - [Build Commands](#build-commands)
  - [Run Commands](#run-commands)
  - [Testing](#testing)
- [Dependencies](#dependencies)
- [Platforms](#building-with-docker-optional)
  - [Docker](#building-with-docker-optional)
  - [macOS](#aprismaos-macos-installation-guide)
  - [Linux / WSL](#aprismaos-linux--wsl-installation-guide)


---

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


## **Features**

What works today (both architectures unless noted):

- **Boot**: Multiboot via GRUB (ISO) or QEMU's `-kernel`; a higher-half kernel on x86_64.
- **Memory**: physical page allocator, paging, a kernel heap (`kmalloc`/`kfree`), and on
  x86_64 one address space per process with the kernel half shared.
- **Scheduling**: preemptive round-robin kernel threads with sleeping and wait queues.
- **Drivers**: VGA text mode with scrollback, PS/2 keyboard with line editing, the PIT timer,
  the CMOS real-time clock (the date and time, UTC; `date` in both shells), and a serial port
  that mirrors all console output (COM1).
- **Filesystem**: one tree in memory. The ramdisk (a tar archive the bootloader loads next to
  the kernel) fills it at boot with `/bin` for programs and files like `/etc/motd`, read-only;
  `/tmp` is writable (files up to 4 MiB, 16 MiB in all, gone after a reboot). Files keep the
  time of their last change (`ls -l`).
- **User mode** (x86_64): programs run in ring 3 and use system calls (`int 0x80`) for
  file descriptors (the console, files and pipes), files and directories (create, write,
  seek, rename, remove, `stat`), a current directory, starting, waiting for and killing
  other programs, sleeping, and growing their heap (`brk`). Faults end the program,
  not the kernel; Ctrl+C ends the foreground process group. x87/SSE registers are saved
  for each process.
- **User space** (x86_64): a C runtime with `malloc`/`free`, a shell (`/bin/sh`) with
  `cd`, pipelines, redirection (`<`, `>`, `>>`, `2>`) and background jobs (`&`, `jobs`, `fg`,
  `kill`), and programs such as `ls`, `cat`, `echo`, `sort`, `wc`, `yes`, `cp`, `mv`, `mkdir`,
  `rm`, `rmdir`, `sleep` and `date`.
- **Kernel shell**: built-in commands for debugging (`help` lists them), also reachable when
  there is no ramdisk.
- **Tests**: host unit tests for libc and kernel helpers, and QEMU tests that boot the system
  and type into it (run in CI on every push).

---

## **Using AprismaOS**

With the ramdisk, the 64-bit build boots into the user-space shell:

```
[SHELL] Starting /bin/sh (type 'exit' for the kernel shell)
$ ls /bin
$ cat /etc/motd
$ echo "two words" and more
$ ls /bin | sort | wc
$ ls /bin > /tmp/list
$ sort < /tmp/list
$ cat > /tmp/notes        (type lines, then Ctrl+D)
$ cd /tmp
$ mv notes old-notes
$ cat missing 2> errors
$ sleep 30 &
$ jobs
$ kill %1
$ date
$ ls -l /tmp
```

Type a program's name and its arguments; `a | b` sends a's output to b, `a < file` lets a
read a file, `a > file` (or `>>` to add to the end) writes one, and `2>` does the same for
error messages. Only `/tmp` is writable. `cd` changes the current directory, which paths
not starting with `/` are relative to; `pwd` shows it. A line ending in `&` runs in the
background: `jobs` lists those jobs, `fg` waits for one and `kill %1` ends one. `help` lists the built-in commands. Ctrl+C ends the running programs (or
starts a new line at the prompt), Ctrl+D ends input, and `exit` (or Ctrl+D at the prompt)
leaves for the kernel shell. There, `help` lists the kernel's own
commands, `run <program> [arguments...]` starts a program and `test` runs self-tests
(e.g. `test user`). The 32-bit build starts in the kernel shell.

Keyboard input goes to the QEMU window; the terminal you started QEMU from shows the serial
log (quit QEMU there with Ctrl+A X).

---

<details>
<summary><h2><b>AprismaOS Development Goals</h2></b></summary>


AprismaOS is a work in progress, with several planned features and enhancements. Below is a categorized list of goals for the project.

---
<details>
<summary><h3><b>Current Focus</h3></b></summary>

- [X] **Basic Output & Debugging**
  - Implement `printf()` for variable printing.
  - Implement an `assert()` function for debugging.

- [X] **Better Platform Compatibility**
  - Improve support for building on different systems and architectures.
  - Resolve cross-platform build issues to ensure smoother development.
  - Improve build procces for implmententation files in libc.

- [X] **32-bit Protected mode**
    - Implement a 32-bit Global Descriptor Table (GDT).

- [X] **32-bit Protected mode**
    - Implement a Task State Segment (TSS).

- [X] **Architecture Support**
  - Support i386/i686 (32-bit x86).
  - Enable transition to **64-bit mode**.

- [X] **Hardware Support**
  - Implement a keyboard driver.
  - Implement cursor control.

- [ ] **Basic OS Services**
  - [X] Implement **file I/O** system calls (reading files from the ramdisk).
  - [ ] Implement a simple **networking stack** (sockets).

- [X] **Memory Management**
  - Create a memory manager.
  - Implement heap allocation (`malloc()` / `free()`, on top of the `brk` system call).

---
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Beginner-Friendly Goals</h3></b></summary>

- [X] **Shell & User Mode**
  - Implement a basic command-line shell (CLI).
  - Support user-space execution (User Mode).

- [X] **Process Management**
  - Introduce process control and basic scheduling (round-robin).

- [ ] **Filesystem & Storage**
  - [ ] Implement a virtual filesystem (VFS) layer.
  - [X] Support basic file reading and writing (in memory: the ramdisk is read-only, `/tmp` writable).

---
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Intermediate Goals</h3></b></summary>

- [X] **Memory & Paging**
  - Implement paging and virtual memory.
  - Introduce memory protection mechanisms.

- [X] **Process & Multitasking**
  - Improve process management with Process Control Blocks (PCBs).
  - Implement context switching and scheduling.

- [ ] **Filesystem Support**
  - Read and write files using **Ext2/Ext4**.
  - Introduce a filesystem driver for real storage support.

- [ ] **Interrupts & Hardware**
  - [X] Improve interrupt handling (exceptions, IRQs).
  - [ ] Implement a basic sound driver for audio output.

- [ ] **Graphical Mode**
  - Implement a simple graphics mode (beyond text mode).

---
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Advanced Goals</h3></b></summary>

- [ ] **System & Libraries**
  - Implement dynamic linking and shared libraries.
  - Introduce a Just-In-Time (JIT) compiler for a toy language.

- [ ] **Performance & Scalability**
  - Support **Symmetric Multi-Processing (SMP)** for multiple CPU cores.
  - Optimize memory and CPU scheduling for better efficiency.

- [ ] **Hardware & Device Support**
  - Implement a USB stack (keyboard, mouse, storage support).
  - Add hypervisor support for virtualization (e.g., KVM).

- [ ] **Networking**
  - Implement a user-space TCP/IP networking stack.

- [ ] **Security**
  - Implement secure boot and kernel signing.
  - Introduce user authentication and permissions.

---
</details>
</details>


<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->



## **Building and Running AprismaOS**

AprismaOS uses the `aprisma.sh` script along with Makefiles to handle building the kernel, creating an ISO, cleaning, and optionally running the system in QEMU.

### **Disclaimer**
Building on different systems and architectures may encounter issues. Resolving these compatibility problems is a priority for future development.

---

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Build Commands</h3></b></summary>

The `--build` option replaces the old `build.sh`:

- **Build the ISO (default)**:
```sh
./aprisma.sh --build iso
```
**Note**: Not required to add `iso` to the command.

- **Build the Kernel**:
```sh
./aprisma.sh --build kernel
```

- **Choose the architecture** (64-bit `x86_64-elf-gcc` is the default; this builds 32-bit):
```sh
./aprisma.sh --compiler i686-elf-gcc --build iso
```
Both architectures share `build/AprismaOS/sysroot`, so building one replaces the other's kernel.

- **Clean**:
```sh
./aprisma.sh --clean
```

- **Editor support** (clangd): write `compile_commands.json` with the flags every file is built
  with (also done by `-d`):
```sh
./aprisma.sh --gen-compile-db
```
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Run Commands</h3></b></summary>

You can optionally run the kernel or ISO after building:

Run the ISO directly in QEMU:
```sh
./aprisma.sh --run iso
```

Run the kernel directly in QEMU:
```sh
./aprisma.sh --run kernel
```
Specify a custom path for kernel or ISO:
```sh
./aprisma.sh --run iso --run-path /path/to/aprisma.iso
./aprisma.sh --run kernel --run-path /path/to/aprisma.kernel
```

Add `--compiler i686-elf-gcc` to build and run the 32-bit system instead. The ramdisk
(`initrd.tar`) is built with the kernel and loaded automatically.

> **Note:** For more help, use the `--help` or `-h` flags with the `aprisma.sh` command.
</details>

<details>
<summary><h3><b>Testing</h3></b></summary>

Host unit tests (libc, pure kernel helpers and the user-space allocator, built with the
host's `gcc`):
```sh
make -C tests/unit
```

QEMU tests boot a built kernel or ISO, read the screen and type into it (Python 3):
```sh
./aprisma.sh --build kernel
python3 scripts/qemu_boot_test.py --arch x86_64 --objcopy x86_64-elf-objcopy \
    --kernel build/AprismaOS/sysroot/boot/aprisma.kernel --run "test user" "passed"
python3 scripts/qemu_keyboard_test.py --arch x86_64 --objcopy x86_64-elf-objcopy \
    --kernel build/AprismaOS/sysroot/boot/aprisma.kernel
```
For the 32-bit system, build with `--compiler i686-elf-gcc` and test with `--arch i386` and
`i686-elf-objcopy`. CI runs all of these for both architectures; see
`.github/workflows/build.yml` for the full set.
</details>


---
<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Dependencies</h3></b></summary>

To build AprismaOS, you need the following:

- **i686-elf and/or x86_64-elf toolchain** (GCC and binutils, with libgcc) – Required for
  building the 32-bit or 64-bit system.
- **GRUB** – Required for the `grub-mkrescue` command, along with the necessary runtime files. _(Needed for building the ISO)_
- **Xorriso** – The ISO creation engine used by `grub-mkrescue`.
- **GNU Make** – Version **4.0 or later**.
- **QEMU** – _(Optional)_ For running and testing the operating system.
- **Python 3** – _(Optional)_ For the QEMU tests and `--gen-compile-db`.
</details>


---
<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
  <summary><h3><b>Building with Docker (Optional)</h3></b></summary>

To simplify setting up a build environment, AprismaOS provides **Docker images**.
Docker is **only for building** the project — running QEMU or a graphical OS inside Docker is **not recommended**.

- Precompiled images are available on Docker Hub: `stre4k/aprisma:latest`.
- You can also build the images locally using the Dockerfiles in the `docker/` directory.

More detailed instructions can be found in the [Docker README](docker/README.md).
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>AprismaOS macOS Installation guide</h3></b></summary>

It is recommended to install all dependencies using [**Homebrew**](https://brew.sh/).

```sh
brew install make xorriso qemu i686-elf-binutils i686-elf-gcc i686-elf-grub x86_64-elf-binutils x86_64-elf-gcc
```

> **Note for MacOS**: To build the ISO, you need to use the `--homebrew-grub` flag with the `aprisma.sh` command.
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>AprismaOS Linux / WSL Installation Guide</h3></b></summary>

This guide explains how to build **AprismaOS** on Linux or Windows Subsystem for Linux (WSL).

#### Installing Dependencies on Ubuntu / WSL

Update package lists and install essential packages:

```bash
sudo apt update
sudo apt install -y build-essential xorriso qemu grub-pc-bin
```

> **Note:** `i686-elf-gcc` and `i686-elf-binutils` are not always available via standard apt repositories. You can either install via a PPA or prebuilt toolchain, or build a custom cross-compiler.

---

#### Building a Custom i686-elf Cross-Compiler

##### 1. Build Binutils

```bash
mkdir -p ~/cross && cd ~/cross
wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz
tar xvf binutils-2.41.tar.gz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=i686-elf --prefix=/usr/local/cross --disable-nls --disable-werror
make
sudo make install
```

##### 2. Build GCC

```bash
cd ~/cross
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar xvf gcc-13.2.0.tar.gz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=i686-elf --prefix=/usr/local/cross --enable-languages=c --disable-nls --without-headers
make all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc
```

The kernel links against libgcc (helpers GCC calls for some operations), so it is built too.

After this, `i686-elf-gcc` should be available globally.

##### 3. For the 64-bit build

Repeat steps 1 and 2 with `--target=x86_64-elf` (in new `build-binutils` and `build-gcc`
directories). After this, `x86_64-elf-gcc` is available as well.

---

#### Configuring the Toolchain

Ensure the cross-compiler is in your `PATH`:

```bash
export PATH=/usr/local/cross/bin:$PATH
```

Verify the installation:

```bash
i686-elf-gcc --version
i686-elf-ld --version
```

---

#### Building AprismaOS

Once the toolchain is installed, you can build AprismaOS (64-bit by default, which needs
the x86_64-elf compiler from step 3):

```bash
sh aprisma.sh --build iso
```

With only the i686-elf compiler, build the 32-bit system:

```bash
sh aprisma.sh --compiler i686-elf-gcc --build iso
```

#### References

- [OSDev Cross Compiler Guide](https://wiki.osdev.org/GCC_Cross-Compiler)

</details>

