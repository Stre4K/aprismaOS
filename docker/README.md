# AprismaOS Docker Toolchain

This directory contains **Dockerfiles** to simplify setting up the AprismaOS build environment.

## Docker Images

- **Dockerfile.base** – Minimal image with compiled cross-compilers (i686-elf / x86_64-elf) and essential libraries.
  ⚠️ **Warning:** Building `base` can take **several hours**, as it compiles GCC, Binutils, and dependencies.

- **Dockerfile.latest** – Extends `base` and adds runtime tools:
  - `make`, `git`
  - `libgmp`, `libmpfr`, `libmpc`
  - `grub-pc-bin`, `xorriso` **(Needed for building the ISO)**

> **Note:** Docker is **only for building** AprismaOS. Running QEMU or a graphical OS inside Docker is not recommended.

> **ARM64 Hosts Note:**
> - On **ARM64 hosts**, `Dockerfile.latest` (`stre4k/aprisma:latest`) only installs kernel/toolchain build tools. ISO creation is **not supported natively** on ARM.
> - To create an ISO on ARM, you have two options:
>   1. Run the **small GRUB image** (`stre4k/aprisma:grub-22.04`) in **QEMU emulation mode**. This image contains only the minimal packages required to create the ISO (`grub-pc-bin`, `xorriso`, `mtools`):
>      ```bash
>      docker run --rm -it \
>          -v $(pwd):/workspace \
>          --platform linux/amd64 \
>          stre4k/aprisma:grub-22.04
>      ```
>      > Inside this container, you can run the ISO creation one-liner:
>      > ```bash
>      > KERNEL=$( [ -f sysroot/boot/aprisma.kernel ] && echo sysroot/boot/aprisma.kernel || echo kernel/aprisma.kernel ); \
>      > mkdir -p isodir/boot/grub && cp "$KERNEL" isodir/boot/aprisma.kernel && \
>      > echo -e "menuentry \"aprisma\" {\n    multiboot /boot/aprisma.kernel root=/dev/null\n}" > isodir/boot/grub/grub.cfg && \
>      > grub-mkrescue -o aprisma.iso isodir && echo "ISO created: aprisma.iso"
>      > ```
>      > **Note:** This requires the kernel image `aprisma.kernel` to be compiled first.
>   2. Alternatively, download the x86 `latest` image (`stre4k/aprisma:latest`) and force emulation using the `--platform linux/amd64` flag:
>      ```bash
>      docker run --rm -it --platform linux/amd64 -v $(pwd):/workspace stre4k/aprisma:latest
>      ```
> - Full ISO creation is currently only guaranteed on **x86 hosts**.

---

## Quickstart

### 1. Pull precompiled images from Docker Hub

- **Latest image (includes runtime tools):**
```bash
docker pull stre4k/aprisma:latest
```

- **Small GRUB-only image (for ISO creation on ARM via emulation):**
```bash
docker pull stre4k/aprisma:grub-22.04
```

### 2. Run images

#### Disposable container
```bash
docker run --rm -it -v $(pwd):/workspace stre4k/aprisma:latest
```

#### Persistent development container
```bash
docker run -it --name aprisma -v $(pwd):/workspace stre4k/aprisma:latest
```

> **Tip:** On ARM64, use the GRUB image for ISO creation:
```bash
docker run --rm -it \
    -v $(pwd):/workspace \
    --platform linux/amd64 \
    stre4k/aprisma:grub-22.04
```

### 3. Build images locally (optional)

```bash
docker build -t aprisma:base -f docker/Dockerfile.base .
docker build -t aprisma:latest -f docker/Dockerfile.latest .
docker build -t aprisma:grub-22.04 -f docker/Dockerfile.grub .
```

### 4. Delete images locally (optional)

```bash
docker rmi aprisma:base
docker rmi aprisma:latest
docker rmi aprisma:grub-22.04
```

If you downloaded precompiled images, you can remove them as well:

```bash
docker rmi stre4k/aprisma:base
docker rmi stre4k/aprisma:latest
docker rmi stre4k/aprisma:grub-22.04
```

> **Note:** To list images locally, run `docker images`.

