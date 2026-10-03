# KEY agent stand-ins (TX Inhibit)

**Design authority:** [docs/TX_INHIBIT.md §3](../docs/TX_INHIBIT.md)  
(Hold sender + KEYing monitor, hang vs hold timeout, race rules)

## Program names

| Name | Role |
|------|------|
| **`inhibit-test`** | **Canonical console.** Cross-platform; installed as `bin/inhibit-test` next to `wsjtx`. |
| **`inhibit-test-gui`** | **Windows GUI.** Grave `` ` `` or mouse. Sends JSON to UDP port 22372. `bin/inhibit-test-gui.exe`. (Source still under `tools/inhibit_spacebar/`.) |
| `send_inhibit_hold.py` | Python stand-in (dev / scripted tests). |

Prefer **`inhibit-test`** (console) or **`inhibit-test-gui`** (Windows GUI) in docs and scripts.

## Build / install

```text
tools/inhibit-test/main.cpp       →  target inhibit-test       →  bin/inhibit-test[.exe]
tools/inhibit_spacebar/*.c        →  target inhibit-test-gui   →  bin/inhibit-test-gui.exe  (Windows only)
```

## Behaviour (both tools)

**KEY key = grave/backtick `` ` ``** (not Space — avoids false holds while typing). GUI also accepts mouse on the big button.

1. **KEY assert** → **hold** immediately (`ttl_ms` = hold_timeout_ms, default 600) + keepalives ~200 ms.  
2. **KEYing monitor** classifies break-in CW vs continuous KEY; measures dit if break-in.  
3. **KEY open** → hang:  
   - **Break-in CW:** hang = **1.5 × word gap** (= 10.5 × dit), clamp ~315–1260 ms (≈40–10 WPM).  
   - **Continuous** (long mark / SSB / non-break-in): hang = **0** → **release hold** immediately.  
4. Hang done → **stop keepalives**, then **`ttl_ms: 0`** (no race).  

### Console (`inhibit-test`)

**Input focus (default):** `` ` `` and q/Esc only count when typed into **this** terminal. Use **`--global-keys`** for system-wide KEY.

**Two behaviours on one key, both always available:**

| Key | Behaviour |
|-----|-----------|
| `` ` `` (unshifted) | **Momentary.** KEY follows the key — assert while held, open on release. What a real KEY line does, and what break-in CW classification needs. |
| `~` (shift+grave) | **Latch on.** The hold stays asserted after you let go. |
| `` ` `` **or** `~` again | **Clears the latch, always.** `` ` `` then continues as momentary while held. |

The latch exists because momentary mode makes some tests impossible: the tool needs
keyboard focus to see the key held, but so does WSJT-X when you want to press Tune or
Enable Tx at the same time. Latch with `~`, slide over to WSJT-X, do what you need,
come back and press either key to release.

A latched KEY reads to the KEYing monitor as **one long continuous mark**, i.e. the
non-break-in / SSB class, so hang is 0 and release is immediate on the next press.
That is right by construction: the operator ends the transmission explicitly rather
than by pausing. Break-in CW hang is still exercised by the unshifted `` ` ``, which
stays momentary.

Note the latch survives loss of focus by design — the hold is held in the tool, not by
the keyboard. Releasing it does need the key press to be seen, so in the default mode
return focus to the terminal, or use `--global-keys` to press from anywhere.

**Keyboard:** the program starts with no special device permission. Windows tracks grave with the console key state. Linux uses `/dev/input` when that device opens, which gives the real key-up time. When it stays closed, each grave byte from this terminal is a 25 ms press. `--global-keys` is system-wide only when that key state is available.

```bash
inhibit-test --station TEST-KEY --ttl-ms 600
inhibit-test --fixed-hang-ms 0
inhibit-test --global-keys             # system-wide when the OS key state is open
```

## Inhibit network

The console tool speaks InhibitStatus (type 17) and TxInhibit (type 18).
WSJT-X sends type 17 to its UDP server.
On the same PC, that server is localhost port 2237.
On a network, this station sends type 17 to multicast `224.0.0.73` port `2237`.
The tool opens one socket for both paths.
Omit `--status-addr` for that default.
`--status-addr` with a unicast IPv4 address listens only on that address.
`--status-addr` with a multicast address joins that group and still receives localhost.
`--status-iface` names the interface for the multicast join.
A failed join leaves the localhost socket open.

One supported source is selected.
The tool sends type 18 only to that sender.
The command uses the source address and source port of the type 17 datagram.
Two or more supported sources print a numbered menu.
Press `1` through `9` in this window to select one.
Type 18 waits until you select.
A second source clears an automatic selection.
A selection you made stays when another source appears.
If the selected source withdraws, the tool clears that selection.
One remaining source is selected again.
Two or more remaining sources ask again.
Digits `1` through `9` select a source.
Grave stays the KEY.
`~` latches.
`q` and Esc quit.
A type 17 with supported=no withdraws that station.
The lease key is `--controller-id` (default `inhibit-test`).
The badge text is `--station` (default `TEST-KEY`).

```bash
inhibit-test
inhibit-test --status-addr 127.0.0.1 --status-port 2237
inhibit-test --status-iface br0
inhibit-test --controller-id SSB-KEY --station ROY-222-SSB
```

A type 17 line shows schema, id, supported, inhibited, source, the four counters, and the two stamps.
`t_rx_ns` is the type 18 read time.
`t_pin_ns` is the pin-drop time.
Both are zero on a heartbeat repeat.
`pin_us` is `(t_pin_ns - t_rx_ns) / 1000` when a pin drop recorded both stamps.
An older type 17 with no stamp fields prints `stamps=absent`.
A `CONFIG` line appears when a station is learned, moves, or withdraws.
A `SELECT` line names the chosen source.
The menu begins with `Select a type 17 source:`.

### Windows GUI (`inhibit-test-gui`)

Keys only when the GUI window is focused. Optional **fixed hang ms** field (empty = KEYing monitor). Esc / Force RELEASE skips hang.
The GUI sends JSON `{"tx_inhibit":...}` to UDP port 22372.

```text
bin\inhibit-test-gui.exe
bin\inhibit-test.exe          optional console sibling (Qt)
```

**Tip:** hold `` ` `` ≥500 ms for hang=0 (continuous). Short taps use break-in hang unless fixed hang is 0.

Requires **Enable TX Inhibit** and RTS/DTR on the WSJT-X station under test.
