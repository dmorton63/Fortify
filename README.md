# Fortify OS

Fortify is a new hobby operating system prototype targeting x86_64 with the Limine boot protocol.

This repository now contains a complete **bring-up skeleton** for the platform features you listed:

- Limine boot integration
- SMP discovery and AP startup callback wiring
- Memory subsystem bootstrap (from Limine memory map)
- USB discovery surface
- Keyboard/mouse input subsystem surface
- Filesystem subsystem surface
- Port manager subsystem with AP deferred maintenance queue
- Video subsystem with animated software 3D demo renderer
- Compositor subsystem (software composition path)
- Networking subsystem surface
- Web access subsystem with AP deferred request queue
- Generic webcam driver with AP deferred frame queue
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
  - Scheduler CPU ownership now resolves the current CPU via SMP LAPIC-ID mapping instead of a hardcoded CPU0 assumption.
  - AP startup entry now registers each AP as online in scheduler per-CPU state and runs a scheduler-aware AP idle loop (`scheduler_service_deferred_switch` + interruptible `hlt`).
  - AP startup now explicitly loads the kernel IDT on each AP before entering interruptible idle.
  - APs now publish an interruptible-ready handshake counter; BSP delays kick attempts until all APs report ready.
  - BSP idle heartbeat now reports SMP online CPU transitions and scheduler-observed online CPU count.
  - Minimal LAPIC fixed-IPI kick plumbing is available from BSP to AP set (`smp_kick_all_aps`) as a cross-CPU wakeup foundation.
  - The SMP kick path now uses a dedicated IDT vector (`0x60`) with a dedicated ISR stub to avoid overlap with PIC IRQ vectors during IPI forensics.
  - AP IPI-kick interrupts now feed into scheduler preemption requests (`scheduler_on_ipi_kick`) so cross-CPU kicks can actively drive deferred scheduling work instead of being observation-only.
  - Scheduler now tracks IPI-triggered scheduler preemption-request attempts per-CPU and in aggregate, with BSP heartbeat logs for uptake telemetry.
  - AP-targeted IPI kicks now enqueue per-CPU scheduler work items into AP-local work queues and route to dedicated AP worker threads (`kernel-ap-worker`), with per-CPU dispatch/execution/drop counters reported in heartbeat telemetry.
  - AP-pinned runnable demo tasks can now be enabled at runtime; tasks are pinned per AP CPU and report per-CPU execution counters in `ap-stats`.
  - AP worker and AP-pinned runnable demo threads now participate in AP-local timeslice rotation during periodic AP kick interrupts, enabling observable AP-side preemption progress.
  - AP work queues now support multiple work kinds via a scheduler dispatch table; one-shot AP maintenance and networking work items execute concrete deferred jobs per AP.
  - AP maintenance/networking work kinds now use periodic requeue with per-kind per-CPU rate limits and pending-depth guards; subsystem-owned producer APIs (`*_request_ap_*`) drive enqueue requests, and subsystem trigger polls (`*_poll_ap_*_trigger`) now decide when to request AP work.
  - Input now includes a minimal real path: keyboard IRQ1 intake into per-AP queues plus AP-deferred processing trigger polling.
  - Input now publishes typed runtime input messages (`key_char`, `key_backspace`, `key_submit`) with metadata (`tick`, `sequence`, scancode/modifier/source fields) for higher-level command/compositor routing.
  - Runtime command handling now stages serial/keyboard input through dedicated per-target runtime event queues and routes typed runtime messages (input vs command targets) with explicit target IDs via a dedicated runtime message-router module before command/compositor dispatch.
  - USB now includes a minimal real path: host IRQ11 intake into per-AP queues plus AP-deferred processing trigger polling.
  - USB AP processing now drains events in batches (up to 8 events/AP job), with processed-event/batch telemetry exposed in `ap-stats`.
  - Storage now includes a minimal real path: disk IRQ14 intake into per-AP queues plus AP-deferred processing trigger polling.
  - Storage AP processing now drains events in batches (up to 8 events/AP job), with processed-event/batch telemetry exposed in `ap-stats`.
  - Filesystem now includes a boot-sector probe slice with FAT16/FAT32/NTFS mountable detection, FAT32 read-only root-directory traversal counters, and FAT32 root file open/read scaffold counters driven by a sector-backed read path (with pluggable external sector-reader callback and synthetic fallback).
  - Networking RX now includes a minimal real path: NIC IRQ10 intake into per-AP queues plus AP-deferred processing trigger polling.
  - Networking RX AP processing now uses adaptive AP batches (8-24 events/AP job based on per-CPU backlog), with processed-packet/batch telemetry exposed in `ap-stats` and per-CPU heartbeat diagnostics.
  - In stress builds, networking RX per-AP queue capacity is increased to reduce synthetic inject drops under burst load.
  - Video software renderer now uses an internal backbuffer (up to 1920x1080) and blits completed frames to reduce visible panel redraw flicker.
  - Audio now includes a minimal real path: audio IRQ5 intake into per-AP queues plus AP-deferred processing trigger polling.
  - Scheduler telemetry now separates enqueue backpressure reasons (`skip rate-limit` vs `skip pending-cap`) in heartbeat logs.
  - BSP heartbeat now emits a concise SMP AP-work self-test summary (`pass`/`pending`) based on per-AP dispatch/execution parity.
  - SMP init now validates APIC readiness (x2APIC-capable MSR path preferred; LAPIC MMIO fallback) and logs readiness state before IPI path use.
  - BSP heartbeat now logs APIC IPI diagnostics (mode/path/target/ICR/ESR/result) for the latest kick attempt to support delivery-path debugging.
  - AP idle-loop wake counters are tracked and reported by BSP heartbeat to confirm post-kick AP execution activity.
- Boot-time logging via Limine terminal service
- Early PMM allocator with guarded memmap-range mode (automatic bootstrap-pool fallback) + framebuffer reservation, and VMM map/unmap/translate API bootstrap
- GNU Make build that compiles and links the kernel ELF
- Minimal freestanding C++ runtime layer (`new/delete`, pure-virtual guard, `.init_array` constructors)

Subsystems are implemented as progressive vertical slices; several (input/USB/storage/networking/audio/video) now execute real AP-deferred work paths with runtime diagnostics, while others remain scaffolded extension points.

## Project Layout

- `boot/` - Limine bootloader config
- `include/fortify/systems/<system>/` - system/subsystem headers grouped by feature
- `src/systems/<system>/` - system/subsystem implementation grouped by feature
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

## Build USB-Bootable ISO

`make image` already produces a hybrid Limine ISO that can be written to USB media.
For an explicit USB artifact target:

```bash
make usb-image
```

Output:

- `build/fortify-usb.iso`

## Run in QEMU

```bash
make run
```

`make run` forwards kernel serial logs to your terminal via `-serial stdio`, and the kernel uses a direct framebuffer text fallback for on-screen output.

To keep serial logs while disabling on-screen framebuffer log text (useful for the video demo), build/run with:

```bash
DISPLAY_LOGS=0 make run
```

(`DISPLAY_LOGS=0` is the default.)

To enable the filesystem external-reader mock path (for validating callback-vs-synthetic sector telemetry):

```bash
FS_MOCK_READER=1 make run-stress
```

(`FS_MOCK_READER=0` is the default and uses synthetic fallback.)

QEMU networking can be enabled for `run`/`run-stress` using an emulated NIC:

```bash
QEMU_NET_ENABLE=1 QEMU_NET_DEVICE=e1000 make run
```

It is disabled by default (for compatibility with QEMU builds that do not include user-mode networking). You can force-disable explicitly with:

```bash
QEMU_NET_ENABLE=0 make run
```

To expose the host `shared/` folder to QEMU as a virtio-9p share:

```bash
QEMU_SHARE_ENABLE=1 make run
```

You can override the host path and mount tag:

```bash
QEMU_SHARE_ENABLE=1 QEMU_SHARE_PATH=/absolute/path/to/share QEMU_SHARE_TAG=fortifyshare make run-stress
```

This prepares a guest-visible shared device for filesystem testing. Fortify now probes for the virtio-9p PCI device during `filesystem_init()`, exposes runtime diagnostics with `fs-9p-probe`, and supports an asynchronous `fs-9p-head` request queue for early transport validation while full mount integration remains in progress.

To bridge a host `README.TXT` into the current filesystem read path (build-time snapshot), enable:

```bash
QEMU_SHARE_ENABLE=1 SHARE_README_BRIDGE=1 QEMU_SHARE_PATH=/absolute/path/to/share make run-stress
```

This injects a README bridge blob (up to 4096 bytes) into the synthetic FAT32 `README.TXT` entry used by `fs-root-head README.TXT`, so you can validate end-to-end host-file ingestion before full 9p guest transport lands. Auto-detect checks `$(QEMU_SHARE_PATH)` first, then falls back to `$(CURDIR)` when the share path has no matching README file.

And pass additional QEMU arguments through `QEMU_EXTRA_FLAGS`:

```bash
QEMU_EXTRA_FLAGS="-d int" make run
```

If interactive serial input is unavailable in your terminal, you can inject one or more
runtime serial commands at launch:

```bash
QEMU_SERIAL_INPUT="fs-root-ls\nfs-root-head KERNEL.BIN\n" make run
```

This sends the commands to COM1 on boot (same path as manual serial typing).
By default, injection waits 2 seconds to avoid early-boot UART input loss while the
kernel is still flooding logs. You can tune this:

```bash
QEMU_SERIAL_INPUT_DELAY=4 QEMU_SERIAL_INPUT="fs-root-ls\n" make run-stress
```

## Stress Mode (AP Work Queue)

Stress mode increases AP enqueue pressure and queue capacity, and logs that mode at boot/runtime. It is tuned to exercise backpressure telemetry (including pending-cap skip paths).

```bash
make run-stress
```

In stress mode, AP-pinned demo tasks now auto-start once all APs are online/ready, so you can validate pinned-task execution from logs even when serial command input is not available.
Stress mode also auto-enables AP-pinned task migration so execution can move across AP CPUs without interactive input.
Scheduler AP worker/AP-pinned thread provisioning scales with SMP CPU count (up to 64 CPUs).

You can also build stress artifacts directly:

```bash
make build-kernel-stress
make image-stress
make usb-image-stress
```

Stress artifacts are built under `build-stress/` to keep them isolated from normal builds.

AP trigger polling is now runtime-profile-tunable through the diagnostic variable
`g_ap_trigger_profile` (`.noinit.diag`):

- `0` = default profile (ports `9/2`, filesystem `7/2`, networking `4/2`, input `1/1`, USB `2/1`, audio `2/1`)
- `1` = balanced profile (ports `7/2`, filesystem `5/2`, networking `2/2`, input `1/1`, USB `1/1`, audio `1/1`)
- `2` = aggressive profile (all subsystems `1/1`)

The BSP heartbeat logs the active profile whenever it changes.

While `make run` is active (`-serial stdio`), you can type runtime commands:

- `ap-trigger` (show current profile)
- `ap-trigger 0` (default)
- `ap-trigger 1` (balanced)
- `ap-trigger 2` (aggressive)
- `ap-stats` (print on-demand AP queue/service snapshot, including AP thread provisioning totals, AP-pinned task counters, AP→AP/BSP→AP context-switch counters, per-AP execution totals, runtime message-router aggregate queue depth/high-water/drop + enqueue/dequeue/route telemetry with route-drop reason counters plus per-target queue/flow counters for input vs command routing, USB/storage/networking processed-event/batch telemetry, and filesystem probe/mount/root-traversal/open-read/sector-reader counters)
- `ap-task-start` (enable AP-pinned runnable demo tasks)
- `ap-task-stop` (disable AP-pinned runnable demo tasks)
- `ap-task-migrate` (show migration toggle state)
- `ap-task-migrate 1` / `ap-task-migrate 0` (enable/disable AP-pinned task migration)
- `ap-migration-smoke [timeout-ticks]` (run deterministic AP-pinned migration validation and emit a one-line pass/fail summary with active/moves/mask/expected/elapsed; default timeout `6000` ticks)
- `smp-log` / `smp-log default|quiet` (show/set periodic SMP observability verbosity; `quiet` suppresses heartbeat and per-CPU chatter while preserving command and self-test transition logs)
- `smp-log-health` / `smp-log-health <ticks>` (show/set quiet-mode health snapshot interval in ticks; `0` disables quiet-mode health snapshots, default `3000`)
- `video-demo` / `video-demo 1|0` (show/enable/disable animated software 3D panel; when disabled, compositor input/command surfaces remain active and continue updating)
- `video-style` / `video-style 0|1|2` (solid / wireframe / combined)
- `video-speed` / `video-speed <1..32>` (frame-time aware rotation speed control)
- `video-font` / `video-font 0|1|2` (text style: mono / bold / shadow)
- `video-text-scale` / `video-text-scale <1..4>` (HUD text scale control)
- `input-kbd` / `input-kbd 0|1` (show/disable/enable PS/2 keyboard event routing into command input; default `1`)
- `net-rx-inject <count>` (inject synthetic NIC RX events to exercise networking AP batch path)
- `net-rx-stats` (print compact networking RX AP pipeline telemetry: queue depth/high-water, IRQ/drop, processed totals split by IRQ vs injected source, and batch/job totals)
- `net-rx-smoke [count]` (inject synthetic networking RX events and emit a one-line pass/fail summary with requested/injected plus immediate IRQ/drop/processed deltas and queue depth; default count `32`)
- `net-rx-smoke-wait [count] [timeout-ticks]` (inject synthetic networking RX events, wait for AP queue drain up to timeout, and emit a one-line pass/fail summary with drain status; defaults: count `32`, timeout `600` ticks)
- `web-head <target> [max-bytes]` (queue an async web head request; max defaults to 256, caps at 4096)
- `web-head-status <request-id>` (check queued web request status: pending/running/done-ok/done-err with bytes/checksum)
- `web-head-stats` (print compact web head-request telemetry: submit/done split by ok/err and pending/running state counts)
- `web-head-smoke-wait <target> [max-bytes] [timeout-ticks]` (queue one web head request and wait for done-ok/done-err up to timeout, actively servicing AP deferred networking jobs while waiting; defaults: max `256`, timeout `600` ticks)
- `usb-inject <count>` (inject synthetic USB host events)
- `usb-stats` (print compact USB AP pipeline telemetry: queue depth/high-water, IRQ/drop, processed totals split by IRQ vs injected source, and batch/job totals)
- `usb-smoke [count]` (inject synthetic USB events and emit a one-line pass/fail summary with requested/injected plus immediate IRQ/drop/processed deltas and queue depth; default count `32`)
- `usb-smoke-wait [count] [timeout-ticks]` (inject synthetic USB events, wait for AP queue drain up to timeout, and emit a one-line pass/fail summary with drain status; defaults: count `32`, timeout `600` ticks)
- `storage-inject <count>` (inject synthetic storage completion events)
- `storage-stats` (print compact storage AP pipeline telemetry: queue depth/high-water, IRQ/drop, processed totals split by IRQ vs injected source, and batch/job totals)
- `storage-smoke [count]` (inject synthetic storage events and emit a one-line pass/fail summary with requested/injected plus immediate IRQ/drop/processed deltas and queue depth; default count `32`)
- `storage-smoke-wait [count] [timeout-ticks]` (inject synthetic storage events, wait for AP queue drain up to timeout, and emit a one-line pass/fail summary with drain status; defaults: count `32`, timeout `600` ticks)
- `audio-inject <count>` (inject synthetic audio buffer events)
- `audio-stats` (print compact audio AP pipeline telemetry: queue depth/high-water, IRQ/drop, processed totals split by IRQ vs injected source, and batch/job totals)
- `audio-smoke [count]` (inject synthetic audio events and emit a one-line pass/fail summary with requested/injected plus immediate IRQ/drop/processed deltas and queue depth; default count `32`)
- `audio-smoke-wait [count] [timeout-ticks]` (inject synthetic audio events, wait for AP queue drain up to timeout, and emit a one-line pass/fail summary with drain status; defaults: count `32`, timeout `600` ticks)
- `webcam-inject <count>` (inject synthetic webcam frame events)
- `webcam-stats` (print compact webcam AP pipeline telemetry: queue depth/high-water, frame/drop, processed totals split by IRQ vs injected source, and batch/job totals)
- `webcam-smoke [count]` (inject synthetic webcam events and emit a one-line pass/fail summary with requested/injected plus immediate frame/drop/processed deltas and queue depth; default count `32`)
- `webcam-smoke-wait [count] [timeout-ticks]` (inject synthetic webcam events, wait for AP queue drain up to timeout, and emit a one-line pass/fail summary with drain status; defaults: count `32`, timeout `600` ticks)
- `inject-all <count>` (inject the same count into network/USB/storage/audio/webcam in one command)
- `fs-root-ls [source]` (list FAT32 root entries from source `1` by default; optional source `0..255`)
- `fs-root-head <name.EXT> [source]` (read up to 256 bytes from a FAT32 root file head and log byte count/checksum)
- `fs-9p-probe` (re-probe PCI for virtio-9p transport and log device count + last BDF/device id)
- `fs-9p-mode [safe|experimental]` (show/set share-mode 9p execution mode; default `safe`; `experimental` is currently blocked in share builds by a lock-guard and reports a warning)
- `fs-9p-head <path> [max-bytes]` (queue an async virtio-9p head read request; max defaults to 256, caps at 2048)
- `fs-9p-head-status <request-id>` (check queued virtio-9p request status: pending/running/done-ok/done-err with bytes/checksum)
- `fs-9p-stats` (print compact virtio-9p head-request telemetry: submit/done split by ok/err, pending/running state counts, completion path split, enqueue stats, and mode-block count)
- `fs-9p-reset-stats` (reset fs-9p head-request telemetry and request queue ids; blocked while active requests are pending/running/claimed)
- `fs-9p-smoke-wait <path> [max-bytes] [timeout-ticks]` (queue one virtio-9p head request and return immediately; completion logs asynchronously as done-ok/done-err/timeout; defaults: max `256`, timeout `600` ticks; with `QEMU_SHARE_ENABLE=1`, requests are safety-gated to avoid AP-worker lockups; when `SHARE_README_BRIDGE=1`, requests use a safe bridge-blob fallback and can report `done-ok`)
- `surface-create <type> <title>` (create a compositor-owned surface; type: `desktop|application|document|workspace|dashboard|tool`; title is a single token, use `_` for spaces)
- `surface-ls` (list compositor surfaces with id/type/state/layer/position/size/title)
- `rmsg-profile [balanced|stress|quiet]` (show/set runtime-message router test profile: backlog warning depth/debounce, backlog-test pause ticks, and smoke-extended backlog count)
- `rmsg-stats` (print a compact runtime-message router snapshot: queue depth/high-water/drop, aggregate flow, drop reasons, and per-target queue/flow)
- `rmsg-verify` (run router counter consistency checks and report `pass|fail` with a failure-bitmask)
- `rmsg-drop-test <src|kbd|unroute>` (inject a synthetic router route-drop reason for telemetry validation: invalid source, keyboard-suppressed, or unroutable target)
- `rmsg-selftest` (run all route-drop probes and assert expected telemetry deltas; reports `pass|fail` plus `delta(src/kbd/unroute/total)`)
- `rmsg-reset-stats` (reset runtime-message router counters/high-water to a clean baseline; blocked while router queue depth is non-zero)
- `rmsg-smoke` (run a compact router smoke sequence: `rmsg-reset-stats` + `rmsg-selftest`, then report a single pass/fail summary line)
- `rmsg-smoke-extended` (run `rmsg-reset-stats` + `rmsg-selftest` + profile-sized backlog injection and report one pass/fail summary line)
- `rmsg-backlog-test <count> [input|cmd|both]` (inject synthetic runtime-message backlog and briefly pause dequeue to exercise router backlog warning/recovery logs; count `1..1024`, mode defaults to `both`)
  - compositor bootstraps `Desktop Surface`, `Workspace Surface`, `Input Surface`, and `Command Center` by default.
  - input is routed through a compositor event handler; keyboard events target the focused surface (defaults to `Input Surface`).
  - serial typing always routes to `Input Surface`; submitted commands update `Command Center` (`LAST` and `STATUS`).
  - PS/2 keyboard typing from the QEMU window is opt-in via `input-kbd 1`.
  - hidden easter key sequence: type `fortify-cube`; type it twice quickly to trigger pulse stage.
  - `make run` now switches interactive terminals to character-at-a-time mode for live serial typing (and restores terminal settings on exit).
- `help` (show command help)

Runtime-message router validation quick checks:

- `rmsg-profile [balanced|stress|quiet]`
  - expected immediate log: current or set `warn-depth/transition/pause/smoke` knobs
  - presets: `balanced` (`16/25/300/16`), `stress` (`8/10/450/64`), `quiet` (`32/50/200/16`)
- `rmsg-stats`
  - expected immediate log: one compact `rmsg-stats` line with aggregate + per-target router counters
  - expected use: quick verification between tests without full `ap-stats` output
- `rmsg-verify`
  - expected immediate log: `pass fail-mask=0x0`
  - expected behavior: validates aggregate-vs-target queue/drop/flow parity and route-drop reason totals
- `rmsg-drop-test src|kbd|unroute`
  - expected immediate log: `injected=<mode>`
  - expected counter delta: `rmsg-drop-reason(<mode>) += 1` and `rmsg(in/cmd/drop).drop += 1`
- `rmsg-selftest`
  - expected immediate log: `pass delta(src/kbd/unroute/total)=.../1/1/1/3`
  - expected counter delta: `rmsg-drop-reason(src/kbd/unroute) += 1/1/1` and route-drop total `+= 3`
- `rmsg-reset-stats`
  - expected immediate log: `reset=ok` (or `reset=blocked pending-depth=<n>` while router messages are still queued)
  - expected counter baseline after success: router queue/drop/flow and route-drop reason counters restart from zero
- `rmsg-smoke`
  - expected immediate log: `pass reset=ok selftest=pass delta(src/kbd/unroute/total)=.../1/1/1/3`
  - expected behavior: one-line health check; fails if reset is blocked or selftest deltas diverge
- `rmsg-smoke-extended`
  - expected immediate log: `pass reset=ok selftest=pass backlog(request/enq input/cmd)=.../.../... delta(src/kbd/unroute/total)=.../1/1/1/3 verify-fail-mask=0x0`
  - expected behavior: one-line reset+selftest+backlog+consistency sanity check using current profile knobs (backlog input payload is synthetic/inert and does not type into `Input Surface`)
- `rmsg-backlog-test <count> [input|cmd|both]`
  - expected immediate log: `requested=<count> enqueued(input/cmd)=<x>/<y>`
  - expected transient logs (debounced ~25 ticks each transition): `runtime message router <input|command> queue backlog warning`, then matching `... backlog recovered`
  - expected counter behavior after drain: `rmsg-target-q(in/cmd)=0/0`, `rmsg-target-hwm(in/cmd)` increased, and no new drops in `rmsg-target-drop(in/cmd)` unless queue capacity is exceeded

USB vertical-slice quick checks:

- `usb-smoke [count]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... queue-depth=...`
  - expected behavior: `injected == requested`, IRQ delta matches request, and queue depth may rise transiently until AP jobs drain it
- `usb-smoke-wait [count] [timeout-ticks]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... drained=1 queue-depth=0 timeout=...`
  - expected behavior: same as `usb-smoke`, plus explicit queue drain confirmation before reporting pass; command actively services USB deferred jobs while waiting
- `usb-stats`
  - expected immediate log: one compact USB line with queue depth/high-water, IRQ/drop, processed(total/irq/inject), and batch/job counters
  - expected behavior after drain: queue depth returns to `0`, processed totals advance, and drop remains `0` unless queue capacity is exceeded

Storage vertical-slice quick checks:

- `storage-smoke [count]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... queue-depth=...`
  - expected behavior: `injected == requested`, IRQ delta matches request, and queue depth may rise transiently until AP jobs drain it
- `storage-smoke-wait [count] [timeout-ticks]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... drained=1 queue-depth=0 timeout=...`
  - expected behavior: same as `storage-smoke`, plus explicit queue drain confirmation before reporting pass; command actively services storage deferred jobs while waiting
- `storage-stats`
  - expected immediate log: one compact storage line with queue depth/high-water, IRQ/drop, processed(total/irq/inject), and batch/job counters
  - expected behavior after drain: queue depth returns to `0`, processed totals advance, and drop remains `0` unless queue capacity is exceeded

Networking RX vertical-slice quick checks:

- `net-rx-smoke [count]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... queue-depth=...`
  - expected behavior: `injected == requested`, IRQ delta matches request, and queue depth may rise transiently until AP jobs drain it
- `net-rx-smoke-wait [count] [timeout-ticks]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... drained=1 queue-depth=0 timeout=...`
  - expected behavior: same as `net-rx-smoke`, plus explicit queue drain confirmation before reporting pass; command actively services networking deferred jobs while waiting
- `net-rx-stats`
  - expected immediate log: one compact networking line with queue depth/high-water, IRQ/drop, processed(total/irq/inject), and batch/job counters
  - expected behavior after drain: queue depth returns to `0`, processed totals advance, and drop remains `0` unless queue capacity is exceeded

Audio vertical-slice quick checks:

- `audio-smoke [count]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... queue-depth=...`
  - expected behavior: `injected == requested`, IRQ delta matches request, and queue depth may rise transiently until AP jobs drain it
- `audio-smoke-wait [count] [timeout-ticks]`
  - expected immediate log: `pass requested/injected=.../... delta(irq/drop/processed)=.../.../... drained=1 queue-depth=0 timeout=...`
  - expected behavior: same as `audio-smoke`, plus explicit queue drain confirmation before reporting pass; command actively services audio deferred jobs while waiting
- `audio-stats`
  - expected immediate log: one compact audio line with queue depth/high-water, IRQ/drop, processed(total/irq/inject), and batch/job counters
  - expected behavior after drain: queue depth returns to `0`, processed totals advance, and drop remains `0` unless queue capacity is exceeded

Webcam vertical-slice quick checks:

- `webcam-smoke [count]`
  - expected immediate log: `pass requested/injected=.../... delta(frame/drop/processed)=.../.../... queue-depth=...`
  - expected behavior: `injected == requested`, frame delta matches request, and queue depth may rise transiently until AP jobs drain it
- `webcam-smoke-wait [count] [timeout-ticks]`
  - expected immediate log: `pass requested/injected=.../... delta(frame/drop/processed)=.../.../... drained=1 queue-depth=0 timeout=...`
  - expected behavior: same as `webcam-smoke`, plus explicit queue drain confirmation before reporting pass; command actively services webcam deferred jobs while waiting
- `webcam-stats`
  - expected immediate log: one compact webcam line with queue depth/high-water, frame/drop, processed(total/irq/inject), and batch/job counters
  - expected behavior after drain: queue depth returns to `0`, processed totals advance, and drop remains `0` unless queue capacity is exceeded

Web head-request vertical-slice quick checks:

- `web-head-smoke-wait <target> [max-bytes] [timeout-ticks]`
  - expected immediate log: `pass id=... state=done-ok bytes=... checksum=... elapsed=... timeout=... tick-stall=0`
  - expected behavior: request submits successfully, reaches `done-ok` before timeout, and reports deterministic byte/checksum telemetry
- `web-head-stats`
  - expected immediate log: one compact webaccess line with submit/done(ok/err) and state(pending/running) counters
  - expected behavior after smoke completion: done counters advance, pending/running return toward `0`

Virtio-9p head-request vertical-slice quick checks:

- `fs-9p-smoke-wait <path> [max-bytes] [timeout-ticks]`
  - expected immediate log: `armed id=... timeout=... (result logs asynchronously)`
  - expected completion log: `pass id=... state=done-ok bytes=... checksum=... elapsed=... timeout=... tick-stall=0`
  - expected behavior: with virtio-9p transport present, request submits successfully and reports deterministic byte/checksum telemetry without blocking command handling
  - share-mode guardrail: `fs-9p-mode` defaults to `safe` (safety-gated path to avoid AP-worker lockups); `fs-9p-mode experimental` is currently blocked in share builds by a lock-guard
  - safe-mode behavior with `QEMU_SHARE_ENABLE=1`: with `SHARE_README_BRIDGE=1`, requests use a safe bridge-blob fallback (`done-ok` when blob is non-empty), otherwise requests complete as `done-err`
  - failure mode when transport is absent: `fail no-device probes/devices=.../...`
- `fs-9p-stats`
  - expected immediate log: one compact filesystem line with submit/done(ok/err), state(pending/running), path(bridge ok/err worker ok/err), enqueue(req/fail), and mode-block counters
  - expected behavior after smoke completion: done counters advance, pending/running return toward `0`
- `fs-9p-reset-stats`
  - expected immediate log: `reset=ok` (or `reset=blocked active=<n>` while requests are still pending/running/claimed)
  - expected behavior after success: fs-9p submit/done/path/enqueue/mode-block counters reset and request ids restart at `1`

AP migration-proof quick checks:

- `ap-migration-smoke [timeout-ticks]`
  - expected immediate log: `armed timeout=... (result logs asynchronously)`
  - expected completion log: `pass active=... moves=... mask=... expected=... elapsed=... timeout=... tick-stall=0`
  - expected behavior: returns immediately (non-blocking), then validates expected AP mask coverage plus nonzero moves when multiple APs are online

If interactive serial input is unavailable, use stress mode and watch periodic log lines:

- `scheduler: AP thread provisioning workers=<provisioned>/<expected> pinned=<provisioned>/<expected>`
- `smp: ap-work kinds ... subsystem-jobs ports/net/fs/usb=... ap-task=<count>`
- `networking: AP RX packets cpu=<cpu> count=<count>` and `networking: AP RX batches cpu=<cpu> count=<count>` (per-CPU RX proof)
- `smp: health ... ap-task(act/exec)=<active>/<executed> mig/mask=<moves>/<cpu-mask> ctx(ap2ap/bsp2ap)=<count>/<count> rmsg(in/cmd/drop)=<count>/<count>/<count> rmsg-drop-reason(src/kbd/unroute)=<count>/<count>/<count> rmsg-hwm=<count> rmsg-target-q(in/cmd)=<depth>/<depth> rmsg-target-hwm(in/cmd)=<hwm>/<hwm> rmsg-target-drop(in/cmd)=<drop>/<drop> rmsg-target-flow(in e/d cmd e/d)=<in-enq>/<in-deq> <cmd-enq>/<cmd-deq> selftest=<pass|pending>`
- `smp: runtime message router input queue backlog warning|recovered` and `smp: runtime message router command queue backlog warning|recovered` (debounced guardrail logs for sustained router queue pressure and drain)
- `smp: self-test migration=pass|pending ...` (compact migration-proof status line)

`smp: health ... selftest=` is now gated by AP work self-test plus migration self-test when
AP-pinned migration validation is active (migration enabled or AP-pinned tasks running).
If one or more CPUs come online late, AP self-test/work dispatch now uses the currently online AP set
instead of stalling behind full CPU-count equality.
When migration validation is inactive, `smp: self-test migration=` reports `pass` with `expected=0`.

## Useful Targets

- `make` or `make build-kernel` - build the kernel ELF
- `make build-kernel-stress` - build kernel ELF with AP stress mode enabled
- `make limine-binaries` - fetch/build Limine artifacts into `build/limine-binaries`
- `make image` - build bootable ISO (auto-fetches Limine binaries if needed)
- `make image-stress` - build bootable ISO with AP stress mode enabled
- `make usb-image` - build USB-bootable ISO artifact (`build/fortify-usb.iso`)
- `make usb-image-stress` - build USB-bootable ISO artifact with AP stress mode enabled
- `make run` - build and run in QEMU
- `make run-share` - build and run in QEMU with host share + README bridge enabled (`QEMU_SHARE_ENABLE=1 SHARE_README_BRIDGE=1 QEMU_SHARE_PATH=$(CURDIR) SHARE_README_FILE=$(CURDIR)/README.md`)
  - bridge blob source auto-detect order: in `QEMU_SHARE_PATH`, `README.TXT`, `README.txt`, `readme.txt`, then `README.md`/`readme.md`; if none exist, the same order is checked under `$(CURDIR)`
- `make run-stress` - build and run in QEMU with AP stress mode enabled
- `make clean` - remove generated outputs

## Optional Script Entry Points

If you prefer scripts directly:

```bash
./scripts/build-kernel.sh
./scripts/create-image.sh . /path/to/limine-binaries
./scripts/run-qemu.sh
```

## Next Milestones

Validated vertical slices:

- USB observability + smoke (`usb-*`) validated
- Storage observability + smoke (`storage-*`) validated
- Networking RX observability + smoke (`net-rx-*`) validated
- Audio observability + smoke (`audio-*`) validated
- Webcam observability + smoke (`webcam-*`) validated

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
