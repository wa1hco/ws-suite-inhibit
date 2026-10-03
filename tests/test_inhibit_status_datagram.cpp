#include <QtTest>
#include <QHostAddress>
#include <QUdpSocket>

#include "tools/inhibit-test/InhibitStatusDatagram.hpp"

class TestInhibitStatusDatagram : public QObject
{
  Q_OBJECT

private slots:
  void hex_fixture_reads_stamps ();
  void short_packet_has_no_stamps ();
  void truncated_stamp_is_rejected ();
  void extra_tail_is_ignored ();
  void other_type_is_ignored ();
  void bad_magic_leaves_fields ();
  void zero_stamps_omit_pin_us ();
  void loopback_receives_type17 ();
  void multicast_receives_type17 ();
  void missing_iface_is_reported ();
  void command_round_trip ();
  void announce_learns_sender ();
  void source_choice_picks_one ();
  void both_paths_one_socket ();
  void inhibit_listen_missing_iface_stays_open ();
};

static InhibitStation test_station (QString const& id, quint16 port)
{
  InhibitStation row;
  row.id = id;
  row.schema = 3;
  row.address = QHostAddress {QStringLiteral ("192.168.1.181")};
  row.port = port;
  row.supported = true;
  return row;
}

static QByteArray hex_type17 ()
{
  // magic, schema 3, type 17, id "AB", supported, inhibited,
  // source "S", hold, release, expiries, invalid, t_rx_ns=1000, t_pin_ns=2500.
  return QByteArray::fromHex (
      "adbccbda"
      "00000003"
      "00000011"
      "00000002" "4142"
      "01"
      "00"
      "00000001" "53"
      "00000001"
      "00000002"
      "00000003"
      "00000004"
      "00000000000003e8"
      "00000000000009c4");
}

void TestInhibitStatusDatagram::hex_fixture_reads_stamps ()
{
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (hex_type17 (), &fields));
  QCOMPARE (fields.id, QStringLiteral ("AB"));
  QCOMPARE (fields.schema, 3u);
  QVERIFY (fields.supported);
  QVERIFY (!fields.inhibited);
  QCOMPARE (fields.source, QStringLiteral ("S"));
  QCOMPARE (fields.hold_rx, 1u);
  QCOMPARE (fields.release_rx, 2u);
  QCOMPARE (fields.expiries, 3u);
  QCOMPARE (fields.invalid, 4u);
  QVERIFY (fields.has_stamps);
  QCOMPARE (fields.t_rx_ns, quint64 {1000});
  QCOMPARE (fields.t_pin_ns, quint64 {2500});
  QCOMPARE (format_inhibit_status (fields),
            QStringLiteral (
                "TYPE17  schema=3  id=\"AB\"  supported=yes  inhibited=no  source=\"S\""
                "  hold_rx=1  release_rx=2  expiries=3  invalid=4"
                "  t_rx_ns=1000  t_pin_ns=2500  pin_us=1"));
}

void TestInhibitStatusDatagram::short_packet_has_no_stamps ()
{
  QByteArray const full = hex_type17 ();
  QByteArray const body = full.left (full.size () - 16);
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (body, &fields));
  QVERIFY (!fields.has_stamps);
  QCOMPARE (fields.invalid, 4u);
  QVERIFY (format_inhibit_status (fields).endsWith (QStringLiteral ("stamps=absent")));
}

void TestInhibitStatusDatagram::truncated_stamp_is_rejected ()
{
  QByteArray const full = hex_type17 ();
  InhibitStatusFields fields;
  fields.id = QStringLiteral ("keep");
  QVERIFY (!read_inhibit_status (full.left (full.size () - 8), &fields));
  QCOMPARE (fields.id, QStringLiteral ("keep"));
}

void TestInhibitStatusDatagram::extra_tail_is_ignored ()
{
  QByteArray datagram = hex_type17 ();
  datagram.append (QByteArray::fromHex ("0000002a"));
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (datagram, &fields));
  QCOMPARE (fields.t_rx_ns, quint64 {1000});
  QCOMPARE (fields.t_pin_ns, quint64 {2500});
}

void TestInhibitStatusDatagram::other_type_is_ignored ()
{
  QByteArray datagram;
  QDataStream out {&datagram, QIODevice::WriteOnly};
  out.setVersion (QDataStream::Qt_5_4);
  out << quint32 {0xadbccbda} << quint32 {3} << quint32 {1}
      << QByteArray {"WS"} << quint32 {15};
  InhibitStatusFields fields;
  QVERIFY (!read_inhibit_status (datagram, &fields));
}

void TestInhibitStatusDatagram::bad_magic_leaves_fields ()
{
  QByteArray datagram = hex_type17 ();
  datagram[0] = '\0';
  InhibitStatusFields fields;
  fields.id = QStringLiteral ("keep");
  QVERIFY (!read_inhibit_status (datagram, &fields));
  QCOMPARE (fields.id, QStringLiteral ("keep"));
  QVERIFY (!read_inhibit_status (datagram, nullptr));
}

void TestInhibitStatusDatagram::zero_stamps_omit_pin_us ()
{
  QByteArray datagram;
  QDataStream out {&datagram, QIODevice::WriteOnly};
  out.setVersion (QDataStream::Qt_5_4);
  out << quint32 {0xadbccbda} << quint32 {3} << quint32 {17}
      << QByteArray {"WS - IC9700"}
      << true << false
      << QByteArray {"TEST-KEY"}
      << quint32 {4} << quint32 {3} << quint32 {1} << quint32 {0}
      << quint64 {0} << quint64 {0};
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (datagram, &fields));
  QVERIFY (fields.has_stamps);
  QCOMPARE (fields.t_rx_ns, quint64 {0});
  QCOMPARE (fields.t_pin_ns, quint64 {0});
  QCOMPARE (fields.id, QStringLiteral ("WS - IC9700"));
  QCOMPARE (fields.source, QStringLiteral ("TEST-KEY"));
  QVERIFY (!format_inhibit_status (fields).contains (QStringLiteral ("pin_us=")));
  QVERIFY (format_inhibit_status (fields).contains (QStringLiteral ("t_rx_ns=0")));
}

static QString expected_hex_line ()
{
  return QStringLiteral (
      "TYPE17  schema=3  id=\"AB\"  supported=yes  inhibited=no  source=\"S\""
      "  hold_rx=1  release_rx=2  expiries=3  invalid=4"
      "  t_rx_ns=1000  t_pin_ns=2500  pin_us=1");
}

void TestInhibitStatusDatagram::loopback_receives_type17 ()
{
  QUdpSocket listen;
  bool open = false;
  QString const note = open_status_listen (listen, QStringLiteral ("127.0.0.1"),
                                           0, QString (), &open);
  QVERIFY2 (open, qPrintable (note));
  QUdpSocket send;
  QByteArray const datagram = hex_type17 ();
  QCOMPARE (send.writeDatagram (datagram, QHostAddress {QHostAddress::LocalHost},
                                listen.localPort ()),
            static_cast<qint64> (datagram.size ()));
  QVERIFY (listen.waitForReadyRead (1000));
  QByteArray got;
  got.resize (static_cast<int> (listen.pendingDatagramSize ()));
  QVERIFY (listen.readDatagram (got.data (), got.size ()) > 0);
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (got, &fields));
  QCOMPARE (format_inhibit_status (fields), expected_hex_line ());
}

void TestInhibitStatusDatagram::multicast_receives_type17 ()
{
  QUdpSocket listen;
  bool open = false;
  QString const note = open_status_listen (listen, QStringLiteral ("224.0.0.73"),
                                           0, QString (), &open);
  QVERIFY2 (open, qPrintable (note));
  QUdpSocket send;
  send.setSocketOption (QAbstractSocket::MulticastTtlOption, 0);
  QByteArray const datagram = hex_type17 ();
  QCOMPARE (send.writeDatagram (datagram, QHostAddress {QStringLiteral ("224.0.0.73")},
                                listen.localPort ()),
            static_cast<qint64> (datagram.size ()));
  QVERIFY2 (listen.waitForReadyRead (1000), qPrintable (note));
  QByteArray got;
  got.resize (static_cast<int> (listen.pendingDatagramSize ()));
  QVERIFY (listen.readDatagram (got.data (), got.size ()) > 0);
  InhibitStatusFields fields;
  QVERIFY (read_inhibit_status (got, &fields));
  QCOMPARE (format_inhibit_status (fields), expected_hex_line ());
}

void TestInhibitStatusDatagram::missing_iface_is_reported ()
{
  QUdpSocket listen;
  bool open = true;
  QString const note = open_status_listen (listen, QStringLiteral ("224.0.0.73"),
                                           0, QStringLiteral ("no-such-iface"), &open);
  QVERIFY (!open);
  QVERIFY (note.contains (QStringLiteral ("no-such-iface")));
}

void TestInhibitStatusDatagram::command_round_trip ()
{
  QByteArray const datagram = encode_tx_inhibit (3, QStringLiteral ("WS - IC9700"),
                                                 QStringLiteral ("inhibit-test"),
                                                 600, QStringLiteral ("TEST-KEY"));
  TxInhibitCommand command;
  QVERIFY (read_tx_inhibit (datagram, &command));
  QCOMPARE (command.schema, 3u);
  QCOMPARE (command.id, QStringLiteral ("WS - IC9700"));
  QCOMPARE (command.controller, QStringLiteral ("inhibit-test"));
  QCOMPARE (command.ttl_ms, 600u);
  QCOMPARE (command.station, QStringLiteral ("TEST-KEY"));

  QByteArray const schema2 = encode_tx_inhibit (2, QStringLiteral ("WS"),
                                                QStringLiteral ("inhibit-test"),
                                                0, QStringLiteral ("TEST-KEY"));
  QVERIFY (read_tx_inhibit (schema2, &command));
  QCOMPARE (command.schema, 2u);
  QCOMPARE (command.ttl_ms, 0u);
  QVERIFY (inhibit_identity_ok (QStringLiteral ("inhibit-test")));
  QVERIFY (!inhibit_identity_ok (QStringLiteral ("has space")));
  QVERIFY (inhibit_station_ok (QStringLiteral ("ROY 222")));
}

void TestInhibitStatusDatagram::announce_learns_sender ()
{
  QHash<QString, InhibitStation> table;
  InhibitStatusFields fields;
  fields.id = QStringLiteral ("WS - IC9700");
  fields.schema = 3;
  fields.supported = true;
  QHostAddress const addr {QStringLiteral ("192.168.1.181")};

  AnnounceResult const first = observe_inhibit_announce (&table, fields, addr, 41234);
  QCOMPARE (static_cast<int> (first.effect), static_cast<int> (AnnounceEffect::Learned));
  QList<InhibitStation> const targets = supported_stations (table);
  QCOMPARE (targets.size (), 1);
  QCOMPARE (targets.at (0).port, quint16 {41234});
  QCOMPARE (targets.at (0).schema, 3u);

  AnnounceResult const again = observe_inhibit_announce (&table, fields, addr, 41234);
  QCOMPARE (static_cast<int> (again.effect), static_cast<int> (AnnounceEffect::Unchanged));

  AnnounceResult const moved = observe_inhibit_announce (&table, fields, addr, 41235);
  QCOMPARE (static_cast<int> (moved.effect), static_cast<int> (AnnounceEffect::Moved));
  QCOMPARE (moved.previous.port, quint16 {41234});
  QCOMPARE (moved.current.port, quint16 {41235});

  fields.id = QStringLiteral ("WS - OTHER");
  AnnounceResult const second = observe_inhibit_announce (&table, fields, addr, 5000);
  QCOMPARE (static_cast<int> (second.effect), static_cast<int> (AnnounceEffect::Learned));
  QCOMPARE (supported_stations (table).size (), 2);

  fields.id = QStringLiteral ("WS - IC9700");
  fields.supported = false;
  AnnounceResult const gone = observe_inhibit_announce (&table, fields, addr, 41235);
  QCOMPARE (static_cast<int> (gone.effect), static_cast<int> (AnnounceEffect::Withdraw));
  QCOMPARE (gone.previous.port, quint16 {41235});
  QCOMPARE (supported_stations (table).size (), 1);

  fields.schema = 1;
  fields.supported = true;
  fields.id = QStringLiteral ("OLD");
  AnnounceResult const rejected = observe_inhibit_announce (&table, fields, addr, 9);
  QCOMPARE (static_cast<int> (rejected.effect), static_cast<int> (AnnounceEffect::Unchanged));
  QCOMPARE (supported_stations (table).size (), 1);
}

void TestInhibitStatusDatagram::source_choice_picks_one ()
{
  SourceChoice choice;
  QList<InhibitStation> one;
  one.append (test_station (QStringLiteral ("A"), 1));

  ChoiceDecision decision = decide_source_choice (&choice, one);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::AutoSelected));
  QCOMPARE (choice.selected_id, QStringLiteral ("A"));
  QVERIFY (!choice.user_picked);
  QVERIFY (decision.release_id.isEmpty ());

  decision = decide_source_choice (&choice, one);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::None));
  QCOMPARE (choice.selected_id, QStringLiteral ("A"));
  QVERIFY (!choice.user_picked);

  QList<InhibitStation> two = one;
  two.append (test_station (QStringLiteral ("B"), 2));
  decision = decide_source_choice (&choice, two);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::Ask));
  QCOMPARE (decision.release_id, QStringLiteral ("A"));
  QVERIFY (choice.selected_id.isEmpty ());
  QVERIFY (!choice.user_picked);

  // Ask already cleared the automatic selection, so the pick has no prior id.
  QString previous;
  QVERIFY (apply_source_pick (&choice, two, 2, &previous));
  QVERIFY (previous.isEmpty ());
  QCOMPARE (choice.selected_id, QStringLiteral ("B"));
  QVERIFY (choice.user_picked);

  SourceChoice switched;
  switched.selected_id = QStringLiteral ("A");
  switched.user_picked = true;
  QVERIFY (apply_source_pick (&switched, two, 2, &previous));
  QCOMPARE (previous, QStringLiteral ("A"));
  QCOMPARE (switched.selected_id, QStringLiteral ("B"));
  QVERIFY (switched.user_picked);

  QList<InhibitStation> three = two;
  three.append (test_station (QStringLiteral ("C"), 3));
  decision = decide_source_choice (&choice, three);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::None));
  QCOMPARE (choice.selected_id, QStringLiteral ("B"));
  QVERIFY (choice.user_picked);

  QList<InhibitStation> without_b;
  without_b.append (test_station (QStringLiteral ("A"), 1));
  without_b.append (test_station (QStringLiteral ("C"), 3));
  decision = decide_source_choice (&choice, without_b);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::Ask));
  QCOMPARE (decision.release_id, QStringLiteral ("B"));
  QVERIFY (choice.selected_id.isEmpty ());
  QVERIFY (!choice.user_picked);

  SourceChoice held;
  held.selected_id = QStringLiteral ("B");
  held.user_picked = true;
  decision = decide_source_choice (&held, one);
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::AutoSelected));
  QCOMPARE (decision.release_id, QStringLiteral ("B"));
  QCOMPARE (decision.selected_id, QStringLiteral ("A"));
  QCOMPARE (held.selected_id, QStringLiteral ("A"));
  QVERIFY (!held.user_picked);

  decision = decide_source_choice (&held, QList<InhibitStation> {});
  QCOMPARE (static_cast<int> (decision.action), static_cast<int> (ChoiceAction::Cleared));
  QCOMPARE (decision.release_id, QStringLiteral ("A"));
  QVERIFY (held.selected_id.isEmpty ());

  QString const menu = format_source_menu (two, QStringLiteral ("B"));
  QVERIFY (menu.contains (QStringLiteral ("Press 1-2.")));
  QVERIFY (menu.contains (QStringLiteral ("id=\"B\"")));
  QVERIFY (menu.contains (QStringLiteral ("selected")));
  QCOMPARE (source_menu_key (two), QStringLiteral ("A\nB"));

  SourceChoice again;
  again.selected_id = QStringLiteral ("B");
  again.user_picked = true;
  QString prior = QStringLiteral ("sentinel");
  QVERIFY (!apply_source_pick (&again, two, 2, &prior));
  QCOMPARE (again.selected_id, QStringLiteral ("B"));
  QVERIFY (again.user_picked);
  QCOMPARE (prior, QStringLiteral ("B"));

  prior = QStringLiteral ("sentinel");
  QVERIFY (!apply_source_pick (&again, two, 0, &prior));
  QVERIFY (!apply_source_pick (&again, two, 3, &prior));
  QCOMPARE (prior, QStringLiteral ("sentinel"));
  QCOMPARE (again.selected_id, QStringLiteral ("B"));
}

void TestInhibitStatusDatagram::both_paths_one_socket ()
{
  QUdpSocket listen;
  bool unicast_open = false;
  bool multicast_joined = false;
  QHostAddress const group {QStringLiteral ("224.0.0.73")};
  QString const note = open_inhibit_listen (listen, 0, group, QString (),
                                            &unicast_open, &multicast_joined);
  QVERIFY2 (unicast_open, qPrintable (note));
  QVERIFY2 (multicast_joined, qPrintable (note));
  quint16 const port = listen.localPort ();
  QVERIFY (port != 0);

  QByteArray const unicast_payload = hex_type17 ();
  QByteArray multicast_payload = unicast_payload;
  multicast_payload[multicast_payload.size () - 1] = 0x11;

  QUdpSocket unicast;
  QCOMPARE (unicast.writeDatagram (unicast_payload,
                                   QHostAddress {QHostAddress::LocalHost}, port),
            static_cast<qint64> (unicast_payload.size ()));
  QVERIFY2 (listen.waitForReadyRead (1000), qPrintable (note));
  QByteArray got;
  got.resize (static_cast<int> (listen.pendingDatagramSize ()));
  QHostAddress sender;
  QVERIFY (listen.readDatagram (got.data (), got.size (), &sender, nullptr) > 0);
  QCOMPARE (got, unicast_payload);
  QCOMPARE (sender, QHostAddress {QHostAddress::LocalHost});
  QVERIFY (!listen.hasPendingDatagrams ());

  QUdpSocket multi;
  multi.setSocketOption (QAbstractSocket::MulticastTtlOption, 0);
  QCOMPARE (multi.writeDatagram (multicast_payload, group, port),
            static_cast<qint64> (multicast_payload.size ()));
  QVERIFY2 (listen.waitForReadyRead (1000), qPrintable (note));
  got.resize (static_cast<int> (listen.pendingDatagramSize ()));
  QVERIFY (listen.readDatagram (got.data (), got.size (), &sender, nullptr) > 0);
  QCOMPARE (got, multicast_payload);
  QVERIFY (sender != group);
}

void TestInhibitStatusDatagram::inhibit_listen_missing_iface_stays_open ()
{
  QUdpSocket listen;
  bool unicast_open = false;
  bool multicast_joined = true;
  QString const note = open_inhibit_listen (
      listen, 0, QHostAddress {QStringLiteral ("224.0.0.73")},
      QStringLiteral ("no-such-iface"), &unicast_open, &multicast_joined);
  QVERIFY2 (unicast_open, qPrintable (note));
  QVERIFY (!multicast_joined);
  QCOMPARE (listen.state (), QAbstractSocket::BoundState);
  QVERIFY (note.contains (QStringLiteral ("no interface")));
  QVERIFY (note.contains (QStringLiteral ("localhost")));

  QByteArray const datagram = hex_type17 ();
  QUdpSocket send;
  QCOMPARE (send.writeDatagram (datagram, QHostAddress {QHostAddress::LocalHost},
                                listen.localPort ()),
            static_cast<qint64> (datagram.size ()));
  QVERIFY2 (listen.waitForReadyRead (1000), qPrintable (note));
  QByteArray got;
  got.resize (static_cast<int> (listen.pendingDatagramSize ()));
  QVERIFY (listen.readDatagram (got.data (), got.size ()) > 0);
  QCOMPARE (got, datagram);
}

QTEST_GUILESS_MAIN (TestInhibitStatusDatagram)
#include "test_inhibit_status_datagram.moc"
