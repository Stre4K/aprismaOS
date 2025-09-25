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
- [Building](#building)
  - [Build Instructions](#build-instructions)
  - [Running the Project](#running-the-project)
- [Dependencies](#dependencies)


---

# **AprismaOS Development Goals**

AprismaOS is a work in progress, with several planned features and enhancements. Below is a categorized list of goals for the project.

---

## **Current Focus**
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

## **Beginner-Friendly Goals**
- [ ] **Shell & User Mode**
  - Implement a basic command-line shell (CLI).
  - Support user-space execution (User Mode).

- [ ] **Process Management**
  - Introduce process control and basic scheduling (round-robin).

- [ ] **Filesystem & Storage**
  - Implement a virtual filesystem (VFS) layer.
  - Support basic file reading and writing.

---

## **Intermediate Goals**
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

## **Advanced Goals**
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

## **Building**

AprismaOS uses shell scripts and Makefiles to build all required files.

### **Disclaimer**
At the moment, building on different systems and architectures may encounter various issues.
Resolving these compatibility problems is a priority for future development.

### **Build Instructions**
Run the following command to build the project:
```sh
sh build.sh
```

### **Running the Project**
Run the following command to start the project:
```sh
sh run.sh
```

## **Dependencies**

To build AprismaOS, you need the following:

- **i686-elf toolchain** – Required for building the kernel.
- **GRUB** – Required for the `grub-mkrescue` command, along with the necessary runtime files. _(Needed for building the ISO, not available on macOS.)_
- **Xorriso** – The ISO creation engine used by `grub-mkrescue`.
- **GNU Make** – Version **4.0 or later**.
- **QEMU** – _(Optional)_ For testing the operating system.

### **macOS Reccomendation**
It is recommended to install all dependencies using [**Homebrew**](https://brew.sh/).

## **Building with Docker (Optional)**

To simplify setting up a build environment, AprismaOS provides **Docker images**.
Docker is **only for building** the project — running QEMU or a graphical OS inside Docker is **not recommended**.

- Precompiled images are available on Docker Hub: `stre4k/aprismaos-toolchain:latest`.
- You can also build the images locally using the Dockerfiles in the `docker/` directory.

More detailed instructions can be found in the [Docker README](docker/README.md).
