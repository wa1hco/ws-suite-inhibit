// Parse and format a WSJT-X InhibitStatus datagram (NetworkMessage type 17).
//
// Header, then: Supported, Inhibited, Source station, Hold rx, Release rx,
// Expiries, Invalid. t_rx_ns and t_pin_ns follow when the sender includes
// them. A datagram that ends after Invalid is still a type 17. Heartbeat
// repeats send both stamps as zero. A pin drop sends both non-zero.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INHIBIT_STATUS_DATAGRAM_HPP
#define INHIBIT_STATUS_DATAGRAM_HPP

#include <algorithm>

#include <QByteArray>
#include <QChar>
#include <QDataStream>
#include <QHash>
#include <QHostAddress>
#include <QIODevice>
#include <QList>
#include <QNetworkInterface>
#include <QString>
#include <QTextStream>
#include <QUdpSocket>
#include <QVector>
#include <QtGlobal>

struct InhibitStatusFields
{
  QString id;
  quint32 schema {0};
  bool supported {false};
  bool inhibited {false};
  QString source;
  quint32 hold_rx {0};
  quint32 release_rx {0};
  quint32 expiries {0};
  quint32 invalid {0};
  bool has_stamps {false};
  quint64 t_rx_ns {0};
  quint64 t_pin_ns {0};
};

// False leaves *fields unchanged. fields must not be null.
inline bool read_inhibit_status (QByteArray const& datagram, InhibitStatusFields * fields)
{
  if (!fields) return false;

  QDataStream in {datagram};
  quint32 magic = 0;
  in >> magic;
  if (in.status () != QDataStream::Ok || magic != 0xadbccbda) return false;

  quint32 schema = 0;
  in >> schema;
  if (in.status () != QDataStream::Ok) return false;
  if (schema <= 1)
    {
      in.setVersion (QDataStream::Qt_5_0);
    }
  else if (schema <= 2)
    {
      in.setVersion (QDataStream::Qt_5_2);
    }
  else if (schema <= 3)
    {
      in.setVersion (QDataStream::Qt_5_4);
    }
  else
    {
      return false;
    }

  quint32 type = 0;
  QByteArray id;
  in >> type >> id;
  if (in.status () != QDataStream::Ok || type != 17) return false;

  bool supported = false;
  bool inhibited = false;
  QByteArray source;
  quint32 hold_rx = 0;
  quint32 release_rx = 0;
  quint32 expiries = 0;
  quint32 invalid = 0;
  in >> supported >> inhibited >> source
     >> hold_rx >> release_rx >> expiries >> invalid;
  if (in.status () != QDataStream::Ok) return false;

  InhibitStatusFields parsed;
  parsed.id = QString::fromUtf8 (id);
  parsed.schema = schema;
  parsed.supported = supported;
  parsed.inhibited = inhibited;
  parsed.source = QString::fromUtf8 (source);
  parsed.hold_rx = hold_rx;
  parsed.release_rx = release_rx;
  parsed.expiries = expiries;
  parsed.invalid = invalid;

  QIODevice * dev = in.device ();
  qint64 const left = dev ? dev->bytesAvailable () : 0;
  if (left == 0)
    {
      *fields = parsed;
      return true;
    }
  if (left < static_cast<qint64> (2 * sizeof (quint64))) return false;

  quint64 t_rx_ns = 0;
  quint64 t_pin_ns = 0;
  in >> t_rx_ns >> t_pin_ns;
  if (in.status () != QDataStream::Ok) return false;
  parsed.has_stamps = true;
  parsed.t_rx_ns = t_rx_ns;
  parsed.t_pin_ns = t_pin_ns;
  *fields = parsed;
  return true;
}

// One display line. No timestamp and no trailing newline.
// pin_us is (t_pin_ns - t_rx_ns) / 1000 when a pin drop recorded both stamps.
inline QString format_inhibit_status (InhibitStatusFields const& fields)
{
  QString line;
  QTextStream ts {&line};
  ts << "TYPE17"
     << "  schema=" << fields.schema
     << "  id=\"" << fields.id << "\""
     << "  supported=" << (fields.supported ? "yes" : "no")
     << "  inhibited=" << (fields.inhibited ? "yes" : "no")
     << "  source=\"" << fields.source << "\""
     << "  hold_rx=" << fields.hold_rx
     << "  release_rx=" << fields.release_rx
     << "  expiries=" << fields.expiries
     << "  invalid=" << fields.invalid;
  if (!fields.has_stamps)
    {
      ts << "  stamps=absent";
    }
  else
    {
      ts << "  t_rx_ns=" << fields.t_rx_ns
         << "  t_pin_ns=" << fields.t_pin_ns;
      if (fields.t_rx_ns > 0 && fields.t_pin_ns >= fields.t_rx_ns)
        {
          ts << "  pin_us=" << ((fields.t_pin_ns - fields.t_rx_ns) / 1000);
        }
    }
  return line;
}

// Controller ID is the type 18 lease key. The station rejects an empty
// value and any space. Station text is the badge and may contain spaces.
inline bool inhibit_identity_ok (QString const& text)
{
  if (text.isEmpty () || text.toUtf8 ().size () > 128) return false;
  if (QString::fromUtf8 (text.toUtf8 ()) != text) return false;
  QVector<uint> const chars = text.toUcs4 ();
  for (int i = 0; i < chars.size (); ++i)
    {
      if (!QChar::isPrint (chars.at (i)) || QChar::isSpace (chars.at (i))) return false;
    }
  return true;
}

inline bool inhibit_station_ok (QString const& text)
{
  if (text.toUtf8 ().size () > 128) return false;
  QVector<uint> const chars = text.toUcs4 ();
  for (int i = 0; i < chars.size (); ++i)
    {
      if (!QChar::isPrint (chars.at (i))) return false;
    }
  return true;
}

// Type 18 TxInhibit. Id, schema, and the destination come from the type 17
// datagram that announced this station. The destination is that datagram's
// sender address and source port.
inline QByteArray encode_tx_inhibit (quint32 schema, QString const& id,
                                     QString const& controller, quint32 ttl_ms,
                                     QString const& station)
{
  QByteArray message;
  QDataStream out {&message, QIODevice::WriteOnly};
  if (schema <= 1)
    {
      out.setVersion (QDataStream::Qt_5_0);
    }
  else if (schema <= 2)
    {
      out.setVersion (QDataStream::Qt_5_2);
    }
  else
    {
      out.setVersion (QDataStream::Qt_5_4);
    }
  out << quint32 {0xadbccbda} << schema << quint32 {18}
      << id.toUtf8 ()
      << controller.toUtf8 ()
      << ttl_ms
      << station.toUtf8 ();
  return message;
}

struct TxInhibitCommand
{
  quint32 schema {0};
  QString id;
  QString controller;
  quint32 ttl_ms {0};
  QString station;
};

inline bool read_tx_inhibit (QByteArray const& datagram, TxInhibitCommand * command)
{
  if (!command) return false;
  QDataStream in {datagram};
  quint32 magic = 0;
  quint32 schema = 0;
  in >> magic >> schema;
  if (in.status () != QDataStream::Ok || magic != 0xadbccbda) return false;
  if (schema <= 1) in.setVersion (QDataStream::Qt_5_0);
  else if (schema <= 2) in.setVersion (QDataStream::Qt_5_2);
  else if (schema <= 3) in.setVersion (QDataStream::Qt_5_4);
  else return false;
  quint32 type = 0;
  QByteArray id;
  QByteArray controller;
  quint32 ttl_ms = 0;
  QByteArray station;
  in >> type >> id >> controller >> ttl_ms >> station;
  if (in.status () != QDataStream::Ok || type != 18) return false;
  TxInhibitCommand parsed;
  parsed.schema = schema;
  parsed.id = QString::fromUtf8 (id);
  parsed.controller = QString::fromUtf8 (controller);
  parsed.ttl_ms = ttl_ms;
  parsed.station = QString::fromUtf8 (station);
  *command = parsed;
  return true;
}

// One learned WSJT-X instance. Commands go to address:port.
struct InhibitStation
{
  QString id;
  quint32 schema {0};
  QHostAddress address;
  quint16 port {0};
  bool supported {false};
};

enum class AnnounceEffect
{
  Unchanged,
  Learned,
  Moved,
  Withdraw
};

struct AnnounceResult
{
  AnnounceEffect effect {AnnounceEffect::Unchanged};
  InhibitStation current;
  InhibitStation previous;
};

inline bool same_inhibit_endpoint (InhibitStation const& a, InhibitStation const& b)
{
  return a.schema == b.schema && a.port == b.port && a.address == b.address;
}

// Record a type 17 announce. A supported station becomes a type 18 target.
// An unsupported announce withdraws a station that was supported.
inline AnnounceResult observe_inhibit_announce (QHash<QString, InhibitStation> * table,
                                                InhibitStatusFields const& fields,
                                                QHostAddress const& sender,
                                                quint16 sender_port)
{
  AnnounceResult result;
  if (!table || fields.id.isEmpty () || sender.isNull () || sender_port == 0)
    {
      return result;
    }
  auto it = table->find (fields.id);
  if (!fields.supported)
    {
      if (it == table->end () || !it->supported) return result;
      result.effect = AnnounceEffect::Withdraw;
      result.previous = *it;
      it->supported = false;
      result.current = *it;
      return result;
    }
  if (fields.schema < 2 || fields.schema > 3) return result;

  InhibitStation next;
  next.id = fields.id;
  next.schema = fields.schema;
  next.address = sender;
  next.port = sender_port;
  next.supported = true;
  if (it == table->end () || !it->supported)
    {
      table->insert (fields.id, next);
      result.effect = AnnounceEffect::Learned;
      result.current = next;
      return result;
    }
  if (!same_inhibit_endpoint (*it, next))
    {
      result.effect = AnnounceEffect::Moved;
      result.previous = *it;
      *it = next;
      result.current = next;
      return result;
    }
  result.effect = AnnounceEffect::Unchanged;
  result.current = *it;
  return result;
}

inline QList<InhibitStation> supported_stations (QHash<QString, InhibitStation> const& table)
{
  QList<InhibitStation> list;
  for (auto it = table.cbegin (); it != table.cend (); ++it)
    {
      if (it->supported && it->port != 0 && !it->address.isNull ())
        {
          list.append (*it);
        }
    }
  std::sort (list.begin (), list.end (), [] (InhibitStation const& a, InhibitStation const& b) {
      return a.id < b.id;
    });
  return list;
}

// The selected type 18 target. One supported source is selected by itself.
// Two or more sources wait for a numbered choice.
struct SourceChoice
{
  QString selected_id;
  bool user_picked {false};
};

enum class ChoiceAction
{
  None,
  AutoSelected,
  Ask,
  Cleared
};

struct ChoiceDecision
{
  ChoiceAction action {ChoiceAction::None};
  QString release_id;
  QString selected_id;
};

inline int inhibit_source_index (QList<InhibitStation> const& stations, QString const& id)
{
  for (int i = 0; i < stations.size (); ++i)
    {
      if (stations.at (i).id == id) return i;
    }
  return -1;
}

inline ChoiceDecision decide_source_choice (SourceChoice * choice,
                                            QList<InhibitStation> const& supported)
{
  ChoiceDecision decision;
  if (!choice) return decision;
  QString const previous = choice->selected_id;
  if (supported.isEmpty ())
    {
      if (!previous.isEmpty ())
        {
          decision.action = ChoiceAction::Cleared;
          decision.release_id = previous;
        }
      choice->selected_id.clear ();
      choice->user_picked = false;
      return decision;
    }
  if (supported.size () == 1)
    {
      if (previous == supported.at (0).id)
        {
          decision.selected_id = previous;
          return decision;
        }
      if (!previous.isEmpty ()) decision.release_id = previous;
      choice->selected_id = supported.at (0).id;
      choice->user_picked = false;
      decision.action = ChoiceAction::AutoSelected;
      decision.selected_id = choice->selected_id;
      return decision;
    }
  int const have = inhibit_source_index (supported, previous);
  if (choice->user_picked && have >= 0)
    {
      decision.selected_id = previous;
      return decision;
    }
  if (!previous.isEmpty ()) decision.release_id = previous;
  choice->selected_id.clear ();
  choice->user_picked = false;
  decision.action = ChoiceAction::Ask;
  return decision;
}

// number is 1-based. At most the first 9 sources are selectable.
// False leaves the choice unchanged. *previous_id receives the prior id.
inline bool apply_source_pick (SourceChoice * choice,
                               QList<InhibitStation> const& supported,
                               int number, QString * previous_id)
{
  if (!choice || !previous_id) return false;
  if (number < 1 || number > supported.size () || number > 9) return false;
  QString const next = supported.at (number - 1).id;
  *previous_id = choice->selected_id;
  if (choice->user_picked && choice->selected_id == next) return false;
  choice->selected_id = next;
  choice->user_picked = true;
  return true;
}

inline bool station_by_id (QHash<QString, InhibitStation> const& table,
                           QString const& id, InhibitStation * station)
{
  if (!station || id.isEmpty ()) return false;
  auto const it = table.constFind (id);
  if (it == table.cend () || !it->supported) return false;
  *station = *it;
  return true;
}

inline QString format_source_menu (QList<InhibitStation> const& stations,
                                   QString const& selected_id)
{
  QString text;
  QTextStream ts {&text};
  int const shown = std::min (stations.size (), 9);
  ts << "Select a type 17 source:\n";
  for (int i = 0; i < shown; ++i)
    {
      InhibitStation const& source = stations.at (i);
      ts << "  " << (i + 1)
         << "  id=\"" << source.id << "\""
         << "  " << source.address.toString () << ':' << source.port
         << "  schema=" << source.schema;
      if (source.id == selected_id) ts << "  selected";
      ts << '\n';
    }
  if (stations.size () > 9)
    {
      ts << "  " << stations.size ()
         << " sources. Keys 1-9 select the first 9.\n";
    }
  ts << "Press 1-" << shown << ".\n";
  return text;
}

inline QString source_menu_key (QList<InhibitStation> const& stations)
{
  QString key;
  int const shown = std::min (stations.size (), 9);
  for (int i = 0; i < shown; ++i)
    {
      if (!key.isEmpty ()) key += QChar ('\n');
      key += stations.at (i).id;
    }
  return key;
}

// Bind the UDP server address. A multicast address also joins the group.
// *open is true only when a datagram can be received. open must not be null.
inline QString open_status_listen (QUdpSocket& sock, QString const& addr_text,
                                   quint16 port, QString const& iface, bool * open)
{
  *open = false;
  QHostAddress const addr {addr_text};
  if (addr.isNull () || addr.protocol () != QAbstractSocket::IPv4Protocol)
    {
      return QStringLiteral ("status listen FAILED: need an IPv4 address");
    }
  QHostAddress const bind_addr = addr.isMulticast () ? QHostAddress {QHostAddress::AnyIPv4}
                                                     : addr;
  auto const mode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
  if (!sock.bind (bind_addr, port, mode))
    {
      return QStringLiteral ("status listen FAILED on %1:%2: %3")
          .arg (bind_addr.toString ())
          .arg (port)
          .arg (sock.errorString ());
    }
  if (addr.isMulticast ())
    {
      bool joined = false;
      if (iface.isEmpty ())
        {
          joined = sock.joinMulticastGroup (addr);
        }
      else
        {
          QNetworkInterface const nif = QNetworkInterface::interfaceFromName (iface);
          if (!nif.isValid ())
            {
              sock.close ();
              return QStringLiteral ("status listen FAILED: no interface %1").arg (iface);
            }
          joined = sock.joinMulticastGroup (addr, nif);
        }
      if (!joined)
        {
          QString const err = sock.errorString ();
          sock.close ();
          return QStringLiteral ("status listen FAILED: join %1:%2: %3")
              .arg (addr.toString ())
              .arg (port)
              .arg (err);
        }
    }
  *open = true;
  QString note = QStringLiteral ("status listen %1:%2 (type 17 InhibitStatus)")
                     .arg (addr.toString ())
                     .arg (port);
  if (!iface.isEmpty ())
    {
      note += QStringLiteral (" iface=") + iface;
    }
  return note;
}

// Receive type 17 on localhost and on a multicast group.
// The socket binds IPv4 any-address, so a datagram to 127.0.0.1:port arrives.
// A multicast group is joined on that same socket.
// A failed join leaves the localhost socket open.
// unicast_open and multicast_joined must not be null.
inline QString open_inhibit_listen (QUdpSocket& sock, quint16 port,
                                    QHostAddress const& group, QString const& iface,
                                    bool * unicast_open, bool * multicast_joined)
{
  *unicast_open = false;
  *multicast_joined = false;
  auto const mode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
  if (!sock.bind (QHostAddress {QHostAddress::AnyIPv4}, port, mode))
    {
      return QStringLiteral ("type 17 listen FAILED on 0.0.0.0:%1: %2")
          .arg (port)
          .arg (sock.errorString ());
    }
  *unicast_open = true;
  QString note = QStringLiteral ("type 17 on localhost:%1").arg (sock.localPort ());
  if (!group.isMulticast ())
    {
      note += QStringLiteral (". multicast join skipped");
      return note;
    }
  bool joined = false;
  QString join_error;
  if (iface.isEmpty ())
    {
      joined = sock.joinMulticastGroup (group);
      if (!joined) join_error = sock.errorString ();
    }
  else
    {
      QNetworkInterface const nif = QNetworkInterface::interfaceFromName (iface);
      if (!nif.isValid ())
        {
          note += QStringLiteral (". multicast join FAILED: no interface %1").arg (iface);
          return note;
        }
      joined = sock.joinMulticastGroup (group, nif);
      if (!joined) join_error = sock.errorString ();
    }
  *multicast_joined = joined;
  if (joined)
    {
      note += QStringLiteral (" and multicast %1:%2")
                  .arg (group.toString ())
                  .arg (sock.localPort ());
      if (!iface.isEmpty ()) note += QStringLiteral (" iface=") + iface;
    }
  else
    {
      note += QStringLiteral (". multicast join FAILED: %1").arg (join_error);
    }
  return note;
}

#endif
