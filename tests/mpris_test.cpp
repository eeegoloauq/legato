// Runs on a private session bus (dbus-run-session, see CMakeLists.txt): what a desktop sees of
// legato over MPRIS, and that its settings reach the player.
#include "mpris.h"
#include "player.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

QVariant busGet(const QString &interface, const QString &name)
{
    QDBusInterface properties(u"org.mpris.MediaPlayer2.legato"_s, u"/org/mpris/MediaPlayer2"_s,
                              u"org.freedesktop.DBus.Properties"_s);
    const QDBusReply<QDBusVariant> reply = properties.call(u"Get"_s, interface, name);
    return reply.isValid() ? reply.value().variant() : QVariant();
}

void busSet(const QString &name, const QVariant &value)
{
    QDBusInterface properties(u"org.mpris.MediaPlayer2.legato"_s, u"/org/mpris/MediaPlayer2"_s,
                              u"org.freedesktop.DBus.Properties"_s);
    properties.call(u"Set"_s, u"org.mpris.MediaPlayer2.Player"_s, name, QVariant::fromValue(QDBusVariant(value)));
}

const auto PlayerInterface = u"org.mpris.MediaPlayer2.Player"_s;

} // namespace

// Receives PropertiesChanged from the bus, as a desktop would.
class Listener : public QObject
{
    Q_OBJECT
public:
    Listener()
    {
        QDBusConnection::sessionBus().connect({}, u"/org/mpris/MediaPlayer2"_s, u"org.freedesktop.DBus.Properties"_s,
                                              u"PropertiesChanged"_s, this,
                                              SLOT(onChanged(QString, QVariantMap, QStringList)));
    }
signals:
    void changed(const QVariantMap &properties);
public slots:
    void onChanged(const QString &, const QVariantMap &properties, const QStringList &) { emit changed(properties); }
};

class MprisTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
    }

    void registersAndShowsNothingPlaying()
    {
        Player player;
        Mpris mpris(&player, nullptr);
        QVERIFY(mpris.registered());
        QCOMPARE(busGet(u"org.mpris.MediaPlayer2"_s, u"Identity"_s).toString(), QCoreApplication::applicationName());
        QCOMPARE(busGet(PlayerInterface, u"PlaybackStatus"_s).toString(), u"Stopped"_s);
        QCOMPARE(busGet(PlayerInterface, u"CanPlay"_s).toBool(), false);
    }

    void loopShuffleAndVolumeReachThePlayer()
    {
        Player player;
        Mpris mpris(&player, nullptr);
        busSet(u"LoopStatus"_s, u"Track"_s);
        QCOMPARE(player.repeat(), 2);
        busSet(u"LoopStatus"_s, u"Playlist"_s);
        QCOMPARE(player.repeat(), 1);
        busSet(u"Shuffle"_s, true);
        QVERIFY(player.shuffle());
        busSet(u"Volume"_s, 0.4);
        QCOMPARE(player.volume(), 40);
        busSet(u"Volume"_s, 7.0);
        QCOMPARE(player.volume(), 100);
    }

    // One PropertiesChanged for several changes in one turn, with only what changed.
    void changesAreSentTogether()
    {
        Player player;
        Mpris mpris(&player, nullptr);
        Listener listener;
        QSignalSpy spy(&listener, &Listener::changed);
        // Signals of the earlier tests may still be on the way.
        while (spy.wait(200)) {
        }
        spy.clear();
        player.setRepeat(2);
        player.setVolume(10);
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.size(), 1);
        const QVariantMap changed = spy.at(0).at(0).toMap();
        QCOMPARE(changed.keys(), (QStringList{u"LoopStatus"_s, u"Volume"_s}));
        // Nothing new: nothing sent.
        player.setVolume(10);
        QVERIFY(!spy.wait(200));
    }
};

QTEST_GUILESS_MAIN(MprisTest)
#include "mpris_test.moc"
