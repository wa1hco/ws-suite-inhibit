#include <QtTest>

#include <pty.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "TxInhibit/TxInhibitDrop.hpp"

// Records each change of the combined pin. Same rule as the inhibit thread:
// pin = ptt_intent AND NOT inhibit, one write per change.
class PinTrace
{
public:
  void set_intent (bool on) { intent_ = on; apply (); }
  void set_inhibit (bool on) { inhibit_ = on; apply (); }
  bool pin () const { return pin_; }
  int writes () const { return writes_; }

private:
  void apply ()
  {
    bool const next = TxInhibitDrop::pin_level (intent_, inhibit_);
    if (writes_ != 0 && next == pin_) return;
    pin_ = next;
    ++writes_;
  }
  bool intent_ {false};
  bool inhibit_ {false};
  bool pin_ {false};
  int writes_ {0};
};

class TestPinCombiner final : public QObject
{
  Q_OBJECT

private slots:
  void level_ ();
  void six_edges_ ();
  void pty_drop_does_not_fake_a_stamp_ ();
  void separate_pty_does_not_fake_a_stamp_ ();
  void separate_port_stays_open_while_selected_ ();
  void idle_hold_has_no_stamp_ ();
  void publish_take_is_one_shot_ ();
};

void TestPinCombiner::level_ ()
{
  QVERIFY (!TxInhibitDrop::pin_level (false, false));
  QVERIFY (TxInhibitDrop::pin_level (true, false));
  QVERIFY (!TxInhibitDrop::pin_level (false, true));
  QVERIFY (!TxInhibitDrop::pin_level (true, true));
}

void TestPinCombiner::six_edges_ ()
{
  PinTrace pin;
  QCOMPARE (pin.writes (), 0);

  pin.set_intent (true);
  QVERIFY (pin.pin ());
  QCOMPARE (pin.writes (), 1);

  pin.set_inhibit (true);
  QVERIFY (!pin.pin ());
  QCOMPARE (pin.writes (), 2);

  pin.set_intent (false);
  QVERIFY (!pin.pin ());
  QCOMPARE (pin.writes (), 2);

  pin.set_intent (true);
  QVERIFY (!pin.pin ());
  QCOMPARE (pin.writes (), 2);

  pin.set_inhibit (false);
  QVERIFY (pin.pin ());
  QCOMPARE (pin.writes (), 3);

  pin.set_inhibit (true);
  QVERIFY (!pin.pin ());
  QCOMPARE (pin.writes (), 4);
  pin.set_inhibit (false);
  QVERIFY (pin.pin ());
  QCOMPARE (pin.writes (), 5);
  pin.set_intent (false);
  QVERIFY (!pin.pin ());
  QCOMPARE (pin.writes (), 6);
}

namespace
{
  struct Pty
  {
    int master {-1};
    int slave {-1};
    char name[128] {};

    bool open ()
    {
      return ::openpty (&master, &slave, name, nullptr, nullptr) == 0;
    }

    ~Pty ()
    {
      if (master >= 0) ::close (master);
      if (slave >= 0) ::close (slave);
    }
  };
}

// A pty rejects TIOCMBIC. The drop must not invent a pin time.
void TestPinCombiner::pty_drop_does_not_fake_a_stamp_ ()
{
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::publish (pty.slave, TIOCM_RTS, false);
  TxInhibitDrop::set_ptt_intent (true);
  TxInhibitDrop::set_inhibit_here (false);

  qint64 const t_rx = TxInhibitDrop::monotonic_ns ();
  TxInhibitDrop::set_inhibit_here (true, t_rx);
  qint64 got_rx = 1;
  qint64 got_pin = 1;
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {0});
  QCOMPARE (got_pin, qint64 {0});
  TxInhibitDrop::shutdown_here ();
}

void TestPinCombiner::separate_pty_does_not_fake_a_stamp_ ()
{
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::set_ptt_intent (true);
  TxInhibitDrop::publish_separate (pty.name, TIOCM_RTS);
  TxInhibitDrop::set_inhibit_here (false);

  qint64 const t_rx = TxInhibitDrop::monotonic_ns ();
  TxInhibitDrop::set_inhibit_here (true, t_rx);
  qint64 got_rx = 1;
  qint64 got_pin = 1;
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {0});
  QCOMPARE (got_pin, qint64 {0});
  TxInhibitDrop::shutdown_here ();
}

void TestPinCombiner::separate_port_stays_open_while_selected_ ()
{
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::publish_separate (pty.name, TIOCM_RTS);
  int const fd = TxInhibitDrop::line_fd ().load ();
  QVERIFY (fd >= 0);

  TxInhibitDrop::set_ptt_intent (true);
  TxInhibitDrop::set_inhibit_here (false);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  TxInhibitDrop::set_inhibit_here (true);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  TxInhibitDrop::set_inhibit_here (false);
  QCOMPARE (TxInhibitDrop::line_fd ().load (), fd);

  TxInhibitDrop::shutdown_here ();
  QCOMPARE (TxInhibitDrop::line_fd ().load (), -1);
  QVERIFY (!TxInhibitDrop::separate_port ().load ());
}

void TestPinCombiner::idle_hold_has_no_stamp_ ()
{
  TxInhibitDrop::shutdown_here ();
  Pty pty;
  QVERIFY (pty.open ());
  TxInhibitDrop::publish (pty.slave, TIOCM_RTS, false);
  TxInhibitDrop::set_ptt_intent (false);
  TxInhibitDrop::set_inhibit_here (false);

  qint64 const t_rx = TxInhibitDrop::monotonic_ns ();
  TxInhibitDrop::set_inhibit_here (true, t_rx);
  qint64 got_rx = 1;
  qint64 got_pin = 1;
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {0});
  QCOMPARE (got_pin, qint64 {0});
  TxInhibitDrop::shutdown_here ();
}

void TestPinCombiner::publish_take_is_one_shot_ ()
{
  TxInhibitDrop::shutdown_here ();
  TxInhibitDrop::publish_pin_stamps (1000, 2500);
  qint64 got_rx = 0;
  qint64 got_pin = 0;
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {1000});
  QCOMPARE (got_pin, qint64 {2500});
  TxInhibitDrop::take_pin_stamps (got_rx, got_pin);
  QCOMPARE (got_rx, qint64 {0});
  QCOMPARE (got_pin, qint64 {0});
  TxInhibitDrop::shutdown_here ();
}

QTEST_MAIN (TestPinCombiner)
#include "test_pin_combiner.moc"
