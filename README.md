# zephyr-lab — RTOS fundamentals on Zephyr (Raspberry Pi Pico 2 W)

## Overview

A small Zephyr application, built up one idea per step, to learn how an RTOS schedules threads and how threads share data safely. Each step adds a single kernel feature and makes its effect visible on the serial console.

Current state (step 3): two threads of different priority add to one shared counter. A `k_mutex` protects it, and once a second `main` prints whether any update was lost.

```text
main 245 + worker 854 = 1099, shared 1099, lost 0
```

---

## Steps

| Step | Idea | Kernel API |
|---|---|---|
| 1 | One thread that sleeps without blocking the rest of the system | `k_msleep()`, `k_uptime_get()` |
| 2 | Two threads, priorities, starvation | `K_THREAD_DEFINE`, `CONFIG_MAIN_THREAD_PRIORITY` |
| 3 | A shared counter, a race condition and its fix | `K_MUTEX_DEFINE`, `k_mutex_lock()`, `k_mutex_unlock()` |

---

## Layout

- `src/main.c`
  The two threads: `main` (priority 10) adds every 7 ms and reports once a second; `worker` (priority 11) adds every 1 ms.

- `src/counter.c`, `inc/counter.h`
  The shared counter. The data and its mutex are `static` in `counter.c`; other files can only use `counter_add()` and `counter_read()`, which both take the lock.

- `inc/zephyr_lab.h`
  Application constants: periods, stack size, worker priority and the artificial delay that makes the race visible.

- `prj.conf`
  Kconfig settings read by Zephyr itself. Only the main thread's priority, which places `main` below the USB driver's thread (priority 8).

- `CMakeLists.txt`
  Adds the `cdc-acm-console` snippet, so `printk` output goes to a USB serial port.

---

## The race, in short

`counter_add()` reads the counter, waits `INCREMENT_DELAY_US` (1 ms), then writes the value plus one. Without the mutex, `main` can preempt `worker` inside that gap and one of the two additions is lost (about 60 per second). With the mutex, `main` waits until `worker` has written and nothing is lost. The delay only widens a gap that exists in every unprotected `count++`.

To see the race, comment out the `k_mutex_lock()` and `k_mutex_unlock()` lines in `counter_add()`.

---

## Build System

### Requirements

- Zephyr 4.5 workspace, set up with `west`
- Zephyr SDK 1.0 (`arm-zephyr-eabi` toolchain)
- Python 3.12 environment with `west`, activated so that `ZEPHYR_BASE` is set

### Build

```bash
west build -b rpi_pico2/rp2350a/m33_0/w     # first build, stores the board in build/
west build                                  # later builds
```

For another board, use a pristine build: `west build -p -b <board>`.

### Outputs

`build/zephyr/` holds `zephyr.elf`, `zephyr.bin`, `zephyr.hex` and `zephyr.uf2`. The image uses about 43 KB of flash and 16.6 KB of RAM, most of it for the USB stack.

---

## Flash and run

1. Hold BOOTSEL while plugging in the board. It appears as a USB drive.
2. Copy the image. The trailing slash makes `cp` fail if the drive is not mounted, instead of creating a file of that name:
   ```bash
   cp build/zephyr/zephyr.uf2 /media/$USER/RP2350/
   ```
3. Open the console:
   ```bash
   screen /dev/ttyACM0
   ```
   Exit with Ctrl-A, K, Y. Without membership in the `dialout` group, prefix `sudo`.

---

## Running without the board

Zephyr's `native_sim` target builds the application as a Linux programme, with the real kernel and scheduler running on simulated time. Timing experiments become repeatable. In a copy of the project, remove the `SNIPPET` line, add `CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000` to `prj.conf` to match the board's tick rate, then:

```bash
west build -b native_sim/native/64 -d build-sim .
./build-sim/zephyr/zephyr.exe --stop_at=5
```

---

## Known issues

| Kind | Where | What |
|---|---|---|
| Intended | `inc/zephyr_lab.h` | `INCREMENT_DELAY_US` holds the mutex for 1 ms on every addition, so `main` can wait up to 1 ms. It exists only to make the race visible. |
| Limit | `src/counter.c` | The counts are 32-bit. At about 550 additions per second, `shared_count` wraps after about 90 days. The `lost` figure stays correct across the wrap, because the subtraction is unsigned. |
