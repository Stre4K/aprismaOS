## Kernel Boot Process Flow

Below is a step-by-step flow of how the system boots and transfers control to the kernel:

```plaintext
   +-------------------+
   | Bootloader Starts |
   |     (e.g., GRUB)  |
   +-------------------+
            |
            v
   +-------------------+
   | Bootloader Loads  |
   | Kernel Image      |
   +-------------------+
            |
            v
   +-------------------+
   | Bootloader Checks |
   | for Multiboot     |
   | Header in Kernel  |
   | (Magic Number:    |
   |  0x1BADB002)      |
   +-------------------+
            |
            v
   +-------------------+
   | Bootloader Loads  |
   | Kernel into Memory|
   | and Prepares      |
   | Multiboot Info    |
   | Structure         |
   +-------------------+
            |
            v
   +-------------------+
   | Bootloader Sets   |
   | Registers:        |
   | - EAX = 0x2BADB002|
   | - EBX = Pointer   |
   |   to Multiboot    |
   |   Info Structure  |
   +-------------------+
            |
            v
   +-------------------+
   | Bootloader Jumps  |
   | to Kernel Entry   |
   | Point             |
   +-------------------+
            |
            v
   +-------------------+
   | Kernel Entry Point|
   | (Assembly Code)   |
   | - Verify EAX      |
   |   (Magic Number)  |
   | - Set Up Stack    |
   | - Call kernel_main|
   +-------------------+
            |
            v
   +-------------------+
   | kernel_main       |
   | (C Function)      |
   | - Parse Multiboot |
   |   Info Structure  |
   | - Initialize      |
   |   Kernel          |
   +-------------------+
            |
            v
   +-------------------+
   | Kernel Takes      |
   | Control of System |
   +-------------------+
```
