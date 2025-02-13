```mermaid
graph TD;
    A[BIOS Starts] --> B["Bootloader (GRUB)"]
    B --> C[Loads Kernel at 1MB]
    C --> D[_start in boot.s]
    D --> E[Setup Stack Pointer]
    E --> F["Call _init (crti.S)"]
    F --> G["Global Constructors (crtbegin.o)"]
    G --> H[Jump to kernel_main]
    H --> I[Run Kernel]
    I -->|"If kernel_main() returns"| J[Call _fini ("crti.S")]
    J --> K["Global Destructors (crtend.o)"]
    K --> L[CPU Halt (hlt)]

    subgraph "C Runtime Initialization"
        F --> G
        J --> K
    end
```


### **Explanation of Flowchart**
1. **BIOS Starts** → Loads GRUB.
2. **GRUB** → Loads the kernel into memory at `1MB`.
3. **`_start` in `boot.s`** runs.
4. **Stack Pointer is initialized**.
5. **`_init` (from `crti.S`) runs**, preparing for global constructors.
6. **`crtbegin.o` inserts global constructor logic**.
7. **Kernel execution starts (`kernel_main()`)**.
8. **If `kernel_main()` returns**, `_fini` runs for global destructors.
9. **`crtend.o` inserts destructor cleanup**.
10. **CPU halts (`hlt`)** to prevent unintended execution.

This flowchart **shows how the bootloader, kernel, and C runtime initialization interact**.


