# AprismaOS Docker Toolchain

This directory contains **Dockerfiles** to simplify setting up the AprismaOS build environment.

## Docker Images

- **Dockerfile.base** – Minimal image with compiled cross-compilers (i686-elf / x86_64-elf) and essential libraries.
  ⚠️ **Warning:** Building `base` can take **several hours**, as it compiles GCC, Binutils, and dependencies.

- **Dockerfile.latest** – Extends `base` and adds runtime tools:
  - `make`, `git`
  - `libgmp`, `libmpfr`, `libmpc`
  - `grub-pc-bin`, `xorriso`

> **Note:** Docker is **only for building** AprismaOS. Running QEMU or a graphical OS inside Docker is not recommended.

---

## Quickstart

### 1. Pull precompiled image from Docker hub (Recommended)
Currently only the `latest` image is available.

```bash
docker pull stre4k/aprismaos-toolchain:latest
```

#### Run (disposable container)
```bash
docker run --rm -it stre4k/aprismaos-toolchain:latest
```

#### Run (persistent dev container)
```bash
docker run -it --name aprismaos-toolchain -v $(pwd):/workspace stre4k/aprismaos-toolchain:latest
```
> **Tip:** After pulling, the image is stored locally, so multiple containers can be run without re-downloading.

### 2. Run image locally (Building is required)
#### Run (disposable container)
```bash
docker run --rm -it aprismaos-toolchain:latest
```

#### Run (persistent dev container)
```bash
docker run -it --name aprismaos-toolchain -v $(pwd):/workspace aprismaos-toolchain:latest
```

### 3. Build images locally (optional)

```bash
docker build -t aprismaos-toolchain:base -f docker/Dockerfile.base .
docker build -t aprismaos-toolchain:latest -f docker/Dockerfile.latest .
```

### 4. Delete images locally (optional)

```bash
docker rmi aprismaos-toolchain:base
docker rmi aprismaos-toolchain:latest
```

If you downloaded the precompiled images, you can delete them as well.

```bash
docker rmi stre4k/aprismaos-toolchain:base (Image is currently not available)
docker rmi stre4k/aprismaos-toolchain:latest
```

> **Note:** To display the image list, run `docker images`.
