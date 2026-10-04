# Results: Ryzen TX Inhibit thread priority

Measured 2026-10-04 on `desktop-jeff` (Ubuntu, AMD Ryzen 7 9800X3D, 16 threads) against `/home/jeff/ham/ws-suite-inhibit/cmake-build-release/ws -r IC9700`. The source of that binary contains the `udp-dispatch` priority request in `TxInhibit/TxInhibitThreadPriority.hpp`. Commit `086922a`.

* Rig: Icom IC-9700. CAT on `/dev/ttyUSB9700a`. PTT method RTS on the separate FTDI adapter. During this run that adapter was `/dev/ttyUSB2`.
* Mode: FT8, dial 144.174 MHz. Tune held the radio in transmit. The Tune watchdog is 90 s. The sweep clicked Tune back on when the watchdog released it. Halt was not required. Tune was off at the end.
* UDP: `127.0.0.1:2237`, Id `WS - IC9700`, Accept UDP requests on.
* Probe: `scratch/probe_ic9700.py --ttl-ms 100 --interval-ms 137 --sweep-s 300`. No `--ptt-port`. Opening the PTT device from the probe asserts RTS and DTR.
* Clock: `CLOCK_MONOTONIC`. `lat_pin_us` is `t_pin_ns - t_rx_ns`. `lat_e2e_us` is `t_pin_ns - t_send`. Wake is `lat_e2e_us - lat_pin_us`. A row with an empty pin stamp did not drop a high pin. The tables below use stamped rows only.

The load was `cmake --build -j16` in `scratch/ws-rebuild-build`, with `CCACHE_DISABLE=1`. That tree is a copy. The running `ws` binary was not replaced. The rebuild loop did not keep all 16 compilers busy for every second. Load average during the sweeps was about 8 to 13. Both arms ran in that same loop.

`udp-dispatch` was class TS for the normal arm and class FF, rtprio 20, for the FIFO arm. Checks during each sweep showed the class held. The thread was left at class FF, rtprio 20.

CSVs:

* [ryzen-rtprio/inhibit-latency-normal.csv](ryzen-rtprio/inhibit-latency-normal.csv)
* [ryzen-rtprio/inhibit-latency-fifo.csv](ryzen-rtprio/inhibit-latency-fifo.csv)

## Normal priority, 5 minutes

Span 295 s. 2055 holds. 1862 holds have a pin stamp.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 1862 | 0.75 ms | 3.0 ms | 15 ms | 286 | 4 | 0 |
| Wake | 1862 | 0.048 ms | 4.6 ms | 6.3 ms | 361 | 13 | 0 |
| Send → pin | 1862 | 0.82 ms | 6.1 ms | 17 ms | 647 | 43 | 0 |

Wake is the time from `sendto` until the pin write starts. The pin write is the ioctl after the thread has the packet. 361 wakes took at least 1 ms. 13 took at least 5 ms. None took 30 ms.

## SCHED_FIFO 20, 5 minutes

Span 296 s. 2064 holds. 1775 holds have a pin stamp. One Tune re-click missed and the next click caught it. That gap is the main reason this arm has fewer stamps.

| | n | p50 | p99 | max | ≥ 1 ms | ≥ 5 ms | ≥ 30 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pin write | 1775 | 0.75 ms | 1.9 ms | 1.9 ms | 286 | 0 | 0 |
| Wake | 1775 | 0.042 ms | 0.077 ms | 0.43 ms | 0 | 0 | 0 |
| Send → pin | 1775 | 0.79 ms | 1.9 ms | 2.0 ms | 312 | 0 | 0 |

No wake took 1 ms. The longest wake was 0.43 ms. Send-to-pin p99 fell from 6.1 ms to 1.9 ms.

## What the pair shows

The benefit is the wake time. The normal arm spent 4.6 ms at the 99th percentile waiting for `udp-dispatch` to run. The FIFO arm spent 0.077 ms. The pin-write median stayed 0.75 ms in both arms. One normal-arm pin write took 15 ms. The FIFO arm did not repeat a pin write that long. That single write is not the scheduling result.

These milliseconds are `CLOCK_MONOTONIC` on this PC. Do not subtract them from a Windows `QueryPerformanceCounter` run.

## Windows run

Run the same pair on the Windows PC after the IC-9700 USB cables move there. The probe has to run on that PC. The pin stamps use that PC's clock.

`DESKTOP-E34PGI3` answered at `192.168.1.72` on 2026-10-04. SSH as `jeff` was refused. The desktop-jeff key and the `dell-inhibit-test` key both failed. Port 22 was open.

The exe measured on 2026-10-02 is `C:\WSJT\ws-suite\build\ws.exe` at commit `834eb8c`. That commit does not call `SetThreadPriority`. `086922a` does. It asks for `THREAD_PRIORITY_HIGHEST` inside the normal process class. It does not change the process class. Pull this tree and run a binary built from that commit.

Use the same probe shape. 300 s, `--ttl-ms 100`, `--interval-ms 137`. Tune holds the pin high. Click Tune again before the 90 s watchdog releases it. Fill the 6 threads with a rebuild in a separate tree, or with one tight loop per core. Do not replace the running `ws.exe`.

Two arms, back to back, under that load:

1. `udp-dispatch` at normal thread priority.
2. `udp-dispatch` at `THREAD_PRIORITY_HIGHEST`.

The new binary raises the thread at startup. Drop that thread to normal for arm 1. Set it back to highest for arm 2. Leave it at highest when the pair ends.

Score wake and the pin write apart. Wake is send-to-pin minus the pin write. Compare the two Windows arms with each other. A 30 ms stall was not present in this Ryzen pair. The result that repeated here is the 1 ms wake count and the wake p99.
