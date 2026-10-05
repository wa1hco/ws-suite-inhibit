# Draft — TX Inhibit for the WS community list

Status: draft. Not sent.

Send to the community list and Uwe, DG2YCB.
Attach `contrib/tx-inhibit-on-ws-3.2.1-260926.patch`.
Base: the inner `ws.tgz` of `ws-3.2.1_260926.tgz`.

---

**Subject:** TX Inhibit for WS 3.2.1 260926 — Timing improvements

---

Hello Uwe and all,

I measured TX Inhibit timing on three PCs: an i7-6700 running
Linux, an i5-8500 running Windows, and a Ryzen 7 9800 running
Linux. Some tests used an idle CPU and some used a CPU running a
WS rebuild. Most inhibit events finish in a few milliseconds, but
two sources of occasional delayed inhibit were identified.

1) Inhibit waited for the CAT message traffic to finish. Waiting
for CAT messages produced delay tails exceeding 30 msec.

2) The GUI loop and OS scheduling sometimes caused delays of about
10 msec.

That is too unpredictable to prevent hot switching the transfer
relay from WS to SSB/CW.

The interval that matters runs from the inhibit request until the
PTT pin changes. WS 3.2.1 260926 asserts inhibit by calling
rig_set_ptt() on the transceiver thread, and that call waits for
any CAT command that already has the rig lock. Fake It causes
extra CAT traffic, inhibit is delayed until the CAT traffic
finishes. There is no reason that RTS or DTR signal changes need
to share the same lock used for CAT messages.

This patch makes several changes:

1) Add a dedicated UDP receive thread to read all incoming UDP.
If a Type 18 inhibit datagram is received, it is acted on
immediately, other UDP datagrams are routed to the GUI thread.

2) The UDP receive thread asks the OS for higher priority to
further reduce delays on a busy CPU. This improved timing on
Linux but had no effect on Windows.

3) The function do_ptt() is used to pass the PTT level to the UDP
thread and no longer calls Hamlib's rig_set_ptt(). The UDP thread
combines the requested PTT level with the inhibit level and
updates the PTT serial port. This approach does not take the rig
lock and so does not wait for CAT traffic that also shares the
rig lock.

4) Improved handling of the case where the serial port for PTT is
different from the serial port used for CAT. The serial port is
opened when selected and held open as long as it is selected
rather than being opened and closed on each PTT transition.

These changes let the inhibit thread drop RTS or DTR as soon as
the inhibit datagram arrives, without waiting for the rig lock or
the GUI. For best timing, use a separate serial port for the RTS
or DTR signal because CAT traffic on a shared USB port causes
delays at the USB framing level. On a separate PTT port, the usual
pin write is about one USB frame, the measured runs stayed under
20 ms, and on Linux a real-time priority for that thread removed
the extra scheduling delay a busy CPU otherwise adds. It's
interesting that on windows running the inhibit thread at elevated
priority was not necessary.

73,
Jeff, wa1hco
