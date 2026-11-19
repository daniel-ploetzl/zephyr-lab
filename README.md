# zephyr-lab: RTOS fundamentals on Zephyr (Raspberry Pi Pico 2 W)

## Overview

A small Zephyr application, built up one idea per step, to learn how an RTOS schedules threads and how threads share data safely. Each step adds a single kernel feature and makes its effect visible on the serial console.

Current state (step 5): two threads of different priority add to one shared counter, which a `k_mutex` protects. A kernel timer sends a sequence number and timestamp to `worker` every 2 ms through a message queue. Once a second `main` reports counter correctness, rejected events and delivery delay.

```text
main 246 + worker 1002 = 1248, shared 1248, lost 0, timer 1003, dropped 0, seq 1002, wait 0 ms
```

---

## Steps

| Step | Idea | Kernel API |
|---|---|---|
| 1 | One thread that sleeps without blocking the rest of the system | `k_msleep()`, `k_uptime_get()` |
| 2 | Two threads, priorities, starvation | `K_THREAD_DEFINE`, `CONFIG_MAIN_THREAD_PRIORITY` |
| 3 | A shared counter, a race condition and its fix | `K_MUTEX_DEFINE`, `k_mutex_lock()`, `k_mutex_unlock()` |
| 4 | A timer interrupt that wakes a thread | `K_TIMER_DEFINE`, `K_SEM_DEFINE`, `k_sem_give()`, `k_sem_take()` |
| 5 | Copy event data from an interrupt to a thread through a bounded queue | `K_MSGQ_DEFINE_STATIC`, `k_msgq_put()`, `k_msgq_get()` |

---

## Layout

- `src/main.c`
  The two threads, the timer and the message queue. `main` (priority 10) adds every 7 ms and reports once a second. The timer's interrupt sends an event every 2 ms and `worker` (priority 11) adds once for each event it receives.

- `src/counter.c`, `inc/counter.h`
  The shared counter. The data and its mutex are `static` in `counter.c`. Other files can only use `counter_add()` and `counter_read()`, which both take the lock.

- `inc/zephyr_lab.h`
  Application constants: periods, stack size, worker priority, queue capacity and the artificial delay that makes the race visible.

- `prj.conf`
  Kconfig settings read by Zephyr itself: the main thread's priority, which places `main` below the USB driver's thread (priority 8), and `CONFIG_ASSERT=y`, which keeps `__ASSERT()` checks in the image.

- `CMakeLists.txt`
  Adds the `cdc-acm-console` snippet, so `printk` output goes to a USB serial port.

---

## The race, in short

`counter_add()` reads the counter, waits `INCREMENT_DELAY_US` (1 ms), then writes the value plus one. Without the mutex, `main` can preempt `worker` inside that gap and one of the two additions is lost. With the mutex, `main` waits until `worker` has written and nothing is lost. The delay only widens a gap that exists in every unprotected `count++`.

To see the race, comment out the `k_mutex_lock()` and `k_mutex_unlock()` lines in `counter_add()`.

---

## Interrupt and thread

The timer's expiry function runs in interrupt context, where nothing may block. It cannot call `counter_add()`, because `k_mutex_lock()` may have to wait. Instead it calls `k_msgq_put()` with `K_NO_WAIT`. `worker` waits in `k_msgq_get()` with `K_FOREVER` and does the work in thread context. The queue replaces step 4's semaphore because receiving a message already wakes the worker.

The queue copies each event's sequence number and creation time. Four events fit in its fixed buffer, using 32 bytes plus the queue object. If it fills, the interrupt drops the newest event and increments `dropped`. There is no allocation or retry loop.

### Reading the report

`lost` checks the shared counter. `dropped` counts rejected events. These measure different things: the counter can stay correct while events are rejected. `seq` is the sequence number most recently published by the worker after completing an addition. `wait` is that worker diagnostic's delivery delay, measured when receiving the event before waiting for the counter mutex.

The diagnostics are individual atomic readings, sampled separately from the counter snapshot and timer status. An event may also be buffered or being processed, so the figures do not form one exact accounting equation. `wait 0 ms` means less than one millisecond on this clock, rather than no delay at all.

### Try overload

1. Set `WORKER_PERIOD_MS` to `1` in `inc/zephyr_lab.h`. Build and run. The CPU cannot handle every event, so `dropped` grows while `lost` stays at 0.
2. Keep that period and change `EVENT_QUEUE_CAPACITY` from `4` to `16`. Compare `wait`: more buffering allows events to become older, without making the worker faster.
3. Restore the defaults before continuing. The annotated mirror's `EXPLAINED.md` has measured results and further exercises.

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

`build/zephyr/` holds `zephyr.elf`, `zephyr.bin`, `zephyr.hex` and `zephyr.uf2`. The step 5 build uses 56,252 bytes of flash and 16,752 bytes of RAM. Most of it is the USB stack. About 13 KB are the kernel's assert checks.

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
| Limit | `src/counter.c` | The counts are 32-bit. At about 620 additions per second, `shared_count` wraps after about 80 days. The `lost` figure stays correct across the wrap, because the subtraction is unsigned. |
| Limit | `src/main.c` | Event timestamps wrap after about 49.7 days. Unsigned delay subtraction works across a wrap if delivery takes less than one full wrap. Sequence numbers also wrap and are diagnostics rather than permanent event identifiers. |
