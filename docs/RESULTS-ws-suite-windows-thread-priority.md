# Results: Windows TX Inhibit thread priority

Measured 2026-10-04 on `DESKTOP-E34PGI3` (Windows, Intel i5-8500T, 6 threads) against `C:\wsjt\ws-suite\build\ws.exe`. That binary was built at 12:40 from commit `fede7cf`. `086922a` asks for `THREAD_PRIORITY_HIGHEST` on `udp-dispatch` and leaves the process class alone.

* Rig: Icom IC-9700. CAT on COM7 (CP210x). PTT method RTS on the separate FTDI adapter, COM6.
* Mode: FT8, dial 144.174 MHz. Tune held the radio in transmit. The Tune watchdog is 90 s. A helper clicked Tune again when the button returned to "Tune". Halt Tx ran at the end of each sweep.
* UDP: `127.0.0.1:2237`, Id `WS`, Accept UDP requests on. Schema 2.
* Probe: 300 s, `--ttl-ms 100`, `--interval-ms 137`. The probe did not open the PTT port.
* Clock: `QueryPerformanceCounter`, converted with the same integer division as `TxInhibitDrop::monotonic_ns()`. `lat_pin_us` is `t_pin_ns - t_rx_ns`. `lat_e2e_us` is `t_pin_ns - t_send`. Wake is `lat_e2e_us - lat_pin_us`. A row with an empty pin stamp did not drop a high pin. The tables below use stamped rows only.

The program raises `udp-dispatch` to `THREAD_PRIORITY_HIGHEST` at startup and leaves the process class alone. The process class stayed `NORMAL_PRIORITY_CLASS` (0x20) on every sweep. The audio threads stayed time-critical.

The two highest-priority sweeps left that thread at priority 2.

The two normal sweeps lowered the same thread after startup with `SetThreadPriority` to `THREAD_PRIORITY_NORMAL` (0). The thread id was 12456. The program raises the thread once, in `UdpDispatchWorker::init`, so it did not raise it again. A check every 15 s found no priority-2 thread, and thread 12456 stayed at priority 0.

Both rebuild arms were `ninja -j 6` of `wsjt_qt` and `wsjt_fort` in `C:\wsjt\ws-rebuild-build`, looped, with `CCACHE_DISABLE=1`. That tree is separate. The running `ws.exe` was not replaced.

CSVs:

* [win-thread-priority/inhibit-latency-idle.csv](win-thread-priority/inhibit-latency-idle.csv) — highest, idle
* [win-thread-priority/inhibit-latency-load.csv](win-thread-priority/inhibit-latency-load.csv) — highest, rebuild
* [win-thread-priority/inhibit-latency-idle-normal.csv](win-thread-priority/inhibit-latency-idle-normal.csv) — normal, idle
* [win-thread-priority/inhibit-latency-load-normal.csv](win-thread-priority/inhibit-latency-load-normal.csv) — normal, rebuild

## Idle desktop, highest priority, 5 minutes

A spot reading before the sweep was 5 percent. No compile was running. Span 300 s. 2039 holds. 2035 holds have a pin stamp. The four empty stamps are the gaps while the 90 s watchdog released Tune and the helper clicked it back on.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 2035 | 0.25 ms | 0.75 ms | 1.4 ms | 2 | 0 | 0 |
| Wake | 2035 | 0.25 ms | 0.87 ms | 1.6 ms | 12 | 0 | 0 |
| Send → pin | 2035 | 0.54 ms | 1.2 ms | 1.8 ms | 82 | 0 | 0 |

Twelve wakes took at least 1 ms. None took 5 ms.

## Six cores rebuilding, highest priority, 5 minutes

Processor load samples every 15 s were 100 percent, except one sample at 29 percent between cycles. Span 300 s. 2029 holds. 1791 holds have a pin stamp. Fourteen empty stamps sit on the first watchdog re-click, near 84 s. One more hold missed a stamp later. The last stamp is at 267 s. The remaining 223 holds, about 33 s, have no pin stamp. The Tune button was still counting down through that tail. Those rows are not in the table.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 1791 | 0.17 ms | 0.40 ms | 0.63 ms | 0 | 0 | 0 |
| Wake | 1791 | 0.19 ms | 0.39 ms | 6.2 ms | 2 | 1 | 0 |
| Send → pin | 1791 | 0.37 ms | 0.68 ms | 6.4 ms | 2 | 1 | 0 |

Two wakes took at least 1 ms. One of those took 6.2 ms, at 260 s. The pin write on that hold was 0.17 ms. No wake took 30 ms.

## Idle desktop, normal priority, 5 minutes

No compile was running. Processor load samples every 15 s were between 6 and 32 percent. Span 300 s. 2057 holds. 1435 holds have a pin stamp. From 80 s to 170 s, 621 holds have no pin stamp. The Tune button was counting down through that gap. One more hold missed a stamp. Those rows are not in the table.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 1435 | 0.24 ms | 0.73 ms | 1.1 ms | 2 | 0 | 0 |
| Wake | 1435 | 0.24 ms | 0.89 ms | 1.7 ms | 9 | 0 | 0 |
| Send → pin | 1435 | 0.51 ms | 1.3 ms | 1.8 ms | 52 | 0 | 0 |

Nine wakes took at least 1 ms. None took 5 ms.

## Six cores rebuilding, normal priority, 5 minutes

Processor load samples every 15 s were 100 percent, except one sample at 83 percent. Span 300 s. 2051 holds. 2035 holds have a pin stamp. Fifteen empty stamps sit in a 2 s gap at 266 s, on a watchdog re-click. One more hold missed a stamp. Those rows are not in the table.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 2035 | 0.17 ms | 0.40 ms | 1.4 ms | 2 | 0 | 0 |
| Wake | 2035 | 0.18 ms | 0.41 ms | 0.81 ms | 0 | 0 | 0 |
| Send → pin | 2035 | 0.36 ms | 0.68 ms | 1.6 ms | 7 | 0 | 0 |

No wake took 1 ms.

## What the highest-priority pair shows

Both arms left `udp-dispatch` at `THREAD_PRIORITY_HIGHEST`. Wake p99 was 0.87 ms with the desktop idle and 0.39 ms while the rebuild held the six cores at 100 percent. The pin-write median moved from 0.25 ms idle to 0.17 ms under the rebuild. The one 6.2 ms wake is a single hold. It is not the shape of the rest of the loaded run.

These milliseconds are `QueryPerformanceCounter` on this PC. Do not subtract them from the Ryzen `CLOCK_MONOTONIC` pair in [RESULTS-ws-suite-ryzen-rtprio.md](RESULTS-ws-suite-ryzen-rtprio.md).

## What the four cells show

Wake time, stamped rows only.

| | Idle p50 | Idle p99 | Idle max | Rebuild p50 | Rebuild p99 | Rebuild max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `THREAD_PRIORITY_HIGHEST` | 0.25 ms | 0.87 ms | 1.6 ms | 0.19 ms | 0.39 ms | 6.2 ms |
| Normal | 0.24 ms | 0.89 ms | 1.7 ms | 0.18 ms | 0.41 ms | 0.81 ms |

Lowering `udp-dispatch` from `THREAD_PRIORITY_HIGHEST` to normal did not move wake time. Idle wake p99 was 0.89 ms at normal priority and 0.87 ms at highest. Under the same rebuild it was 0.41 ms at normal and 0.39 ms at highest. Pin-write p50 stayed near 0.24 ms idle and 0.17 ms while the six cores were busy.

The normal rebuild had no wake at or above 1 ms. Its longest wake was 0.81 ms. The 6.2 ms wake in the highest-priority rebuild was one hold.

The same clock warning applies to all four cells. Do not subtract these `QueryPerformanceCounter` milliseconds from the Ryzen pair.
