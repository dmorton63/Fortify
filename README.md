# Fortify OS

Fortify is a new hobby operating system prototype targeting x86_64 with the Limine boot protocol.

This repository now contains a complete **bring-up skeleton** for the platform features you listed:

- Limine boot integration
- SMP discovery and AP startup callback wiring
- Memory subsystem bootstrap (from Limine memory map)
- USB discovery surface
- Keyboard/mouse input subsystem surface
- Filesystem subsystem surface
- Port manager subsystem surface
- Video subsystem + software renderer surface
- Compositor subsystem surface
- Networking subsystem surface
- Web access subsystem surface
- Generic webcam driver surface
- Generic audio subsystem surface
- TPM subsystem surface
- Secure boot policy subsystem surface
- Quantum simulator demo subsystem

## Current Status

This is a **real kernel scaffold**, not a mockup:

- Freestanding C++ kernel entrypoint via Limine
- Structured subsystem initialization pipeline
- IDT installation with exception panic diagnostics
- Legacy PIC remap with PIT timer tick source (100Hz scaffold)
- ISR dispatcher call boundary hardened with a dedicated IRQ-dispatch stack in assembly stubs
- Minimal round-robin scheduler scaffold with timer-driven preemption, split saved-vs-return interrupt-stack-image ownership (dedicated per-thread saved image + materialized iret return slot), explicit post-`iretq` `rsp/ss` resume slots in per-thread stack frames, and BSP worker-thread stack handoff scaffolding (worker context currently parked; timer IRQ stages pending switches and idle-loop thread context drains deferred commits)
  - Optional guarded worker-unpark/entry probes are available in scheduler code for debugging first-worker resume issues (disabled by default for quiet baseline boots and gated behind a single scheduler/interrupt `kForensicsMode` switch).
- Boot-time logging via Limine terminal service
- Early PMM allocator with guarded memmap-range mode (automatic bootstrap-pool fallback) + framebuffer reservation, and VMM map/unmap/translate API bootstrap
- GNU Make build that compiles and links the kernel ELF
- Minimal freestanding C++ runtime layer (`new/delete`, pure-virtual guard, `.init_array` constructors)

Subsystems are currently implemented as bootable stubs with clear extension points.

## Project Layout

- `boot/` - Limine bootloader config
- `include/fortify/` - kernel headers
- `src/kernel/` - core kernel and Limine glue
- `src/subsystems/` - subsystem modules
- `build/` - linker script and generated artifacts
- `scripts/` - optional Bash helper scripts for direct Linux/WSL usage

## Tooling

For a WSL/Linux workflow, install:

- `git` (for auto-fetching/building Limine binaries if needed)
- `xorriso`
- `qemu-system-x86_64`

## Build Kernel ELF

```bash
make build-kernel
```

Output:

- `build/fortify.elf`

## Build Bootable ISO

By default, `make image` will auto-clone and build Limine into `build/limine-binaries` if binaries are missing.

```bash
make image
```

You can also provide your own Limine binary directory:

```bash
make image LIMINE_BINARIES=/path/to/limine-binaries
```

Output:

- `build/fortify.iso`

## Run in QEMU

```bash
make run
```

`make run` forwards kernel serial logs to your terminal via `-serial stdio`, and the kernel uses a direct framebuffer text fallback for on-screen output.

## Useful Targets

- `make` or `make build-kernel` - build the kernel ELF
- `make limine-binaries` - fetch/build Limine artifacts into `build/limine-binaries`
- `make image` - build bootable ISO (auto-fetches Limine binaries if needed)
- `make run` - build and run in QEMU
- `make clean` - remove generated outputs

## Optional Script Entry Points

If you prefer scripts directly:

```bash
./scripts/build-kernel.sh
./scripts/create-image.sh . /path/to/limine-binaries
./scripts/run-qemu.sh
```

## Next Milestones

1. Wire VMM APIs to real hardware page tables (CR3-backed mappings)
2. Add robust memmap pointer/probe validation to eliminate PMM fallback path
3. Replace scheduler scaffold with real context switching and per-CPU run queues
4. Driver model and PCI enumeration
5. VFS with one concrete filesystem implementation
6. Userspace ABI and process model
7. GPU/renderer acceleration path beyond software renderer

## C++ Runtime Policy

- Exceptions and RTTI are disabled for kernel safety and binary size.
- A temporary early-boot allocator backs global `new`/`delete`.
- Global constructors are supported through `.init_array` and are called during early kernel boot.
- Replace the early allocator with the real memory subsystem allocator once the VM/heap manager is online.
