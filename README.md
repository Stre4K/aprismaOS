# **Welcome to AprismaOS!**

Welcome to **AprismaOS**, my grand project to build my own operating system, **Aprisma**!
This project is a **work in progress** and subject to major changes.

AprismaOS is primarily a **learning project** focused on OS kernel development and
gaining experience with lower-level programming. It currently targets the **i386-elf/i686-elf**
architecture (32-bit x86) but focus is on implementing **64-bit mode**.

---

# **Table of Contents**
- [AprismaOS Development Goals](#aprismaos-development-goals)
  - [Current Focus](#current-focus)
  - [Beginner-Friendly Goals](#beginner-friendly-goals)
  - [Intermediate Goals](#intermediate-goals)
  - [Advanced Goals](#advanced-goals)
- [Building and Running](#building-and-running-aprismaos)
  - [Build Commands](#build-commands)
  - [Run Commands](#run-commands)
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


<details>
<summary><h2><b>AprismaOS Development Goals</h2></b></summary>


AprismaOS is a work in progress, with several planned features and enhancements. Below is a categorized list of goals for the project.

---
<details>
<summary><h3><b>Current Focus</h3></b></summary>

- [X] **Basic Output & Debugging**
  - Implement `printf()` for variable printing.
  - Implement an `assert()` function for debugging.

- [ ] **Better Platform Compatibility**
  - Improve support for building on different systems and architectures.
  - Resolve cross-platform build issues to ensure smoother development.
  - Improve build procces for implmententation files in libc.

- [X] **32-bit Protected mode**
    - Implement a 32-bit Global Descriptor Table (GDT).

- [ ] **32-bit Protected mode**
    - Implement a Task State Segment (TSS).

- [ ] **Architecture Support**
  - Support i386/i686 (32-bit x86).
  - Enable transition to **64-bit mode**.

- [ ] **Hardware Support**
  - Implement a keyboard driver.
  - Implement cursor control.

- [ ] **Basic OS Services**
  - Implement **file I/O** system calls.
  - Implement a simple **networking stack** (sockets).

- [ ] **Memory Management**
  - Create a memory manager.
  - Implement heap allocation (`malloc()` / `free()` system calls).

---
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Beginner-Friendly Goals</h3></b></summary>

- [ ] **Shell & User Mode**
  - Implement a basic command-line shell (CLI).
  - Support user-space execution (User Mode).

- [ ] **Process Management**
  - Introduce process control and basic scheduling (round-robin).

- [ ] **Filesystem & Storage**
  - Implement a virtual filesystem (VFS) layer.
  - Support basic file reading and writing.

---
</details>

<!--
=========================================================================================================================================================
=========================================================================================================================================================
=========================================================================================================================================================
-->


<details>
<summary><h3><b>Intermediate Goals</h3></b></summary>

- [ ] **Memory & Paging**
  - Implement paging and virtual memory.
  - Introduce memory protection mechanisms.

- [ ] **Process & Multitasking**
  - Improve process management with Process Control Blocks (PCBs).
  - Implement context switching and scheduling.

- [ ] **Filesystem Support**
  - Read and write files using **Ext2/Ext4**.
  - Introduce a filesystem driver for real storage support.

- [ ] **Interrupts & Hardware**
  - Improve interrupt handling (exceptions, IRQs).
  - Implement a basic sound driver for audio output.

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

- **Clean**:
```sh
./aprisma.sh --clean
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

> **Note:** For more help, use the `--help` or `-h` flags with the `aprisma.sh` command.
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

- **i686-elf toolchain** – Required for building the kernel.
- **GRUB** – Required for the `grub-mkrescue` command, along with the necessary runtime files. _(Needed for building the ISO)_
- **Xorriso** – The ISO creation engine used by `grub-mkrescue`.
- **GNU Make** – Version **4.0 or later**.
- **QEMU** – _(Optional)_ For testing the operating system.
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

- Precompiled images are available on Docker Hub: `stre4k/aprismaos-toolchain:latest`.
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
make all-gcc
sudo make install-gcc
```

After this, `i686-elf-gcc` should be available globally.

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

Once the toolchain is installed, you can build AprismaOS:

```bash
sh aprisma.sh --build iso
```

#### References

- [OSDev Cross Compiler Guide](https://wiki.osdev.org/GCC_Cross-Compiler)

</details>

