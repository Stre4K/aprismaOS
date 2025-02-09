# **Welcome to AprismaOS!**

Welcome to **AprismaOS**, my grand project to build my own operating system, **Aprisma**!
This project is a **work in progress** and subject to major changes.

AprismaOS is built for the **i386-elf/i686-elf** architecture, which is the 32-bit version of x86.

---

## **Building**

AprismaOS uses shell scripts and Makefiles to build all required files.

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

- **i686-elf toolchain** – (As discussed above).
- **GRUB** – Required for the `grub-mkrescue` command, along with the necessary runtime files. _(Needed for building the ISO, not available on macOS.)_
- **Xorriso** – The ISO creation engine used by `grub-mkrescue`.
- **GNU Make** – Version **4.0 or later**.
- **QEMU** – _(Optional)_ For testing the operating system.

