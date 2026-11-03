#include "serverlist.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

class ServerListTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsThroughSettings()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"settings.conf"_s);
        ServerList list;
        list.put({u"a"_s, u"Home"_s, u"https://music.example.org"_s, u"me"_s});
        list.put({u"b"_s, u"studio.local"_s, u"http://studio.local:4533"_s, u"me"_s});
        list.activeId = u"b"_s;
        {
            QSettings settings(path, QSettings::IniFormat);
            list.write(settings);
        }
        QSettings settings(path, QSettings::IniFormat);
        const ServerList read = ServerList::read(settings);
        QCOMPARE(read.servers.size(), 2);
        QCOMPARE(read.active()->address, u"http://studio.local:4533"_s);
        QCOMPARE(read.find(u"a"_s)->name, u"Home"_s);
    }

    void removingShrinksTheSavedList()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        ServerList list;
        list.put({u"a"_s, u"A"_s, u"https://a.example"_s, u"me"_s});
        list.put({u"b"_s, u"B"_s, u"https://b.example"_s, u"me"_s});
        list.activeId = u"a"_s;
        list.write(settings);
        list.remove(u"a"_s);
        QVERIFY(list.activeId.isEmpty());
        list.write(settings);
        QCOMPARE(ServerList::read(settings).servers.size(), 1);
    }

    void putReplacesByIdAndMatchFindsByAddress()
    {
        ServerList list;
        list.put({u"a"_s, u"A"_s, u"https://a.example"_s, u"me"_s});
        list.put({u"a"_s, u"Renamed"_s, u"https://a.example"_s, u"me"_s});
        QCOMPARE(list.servers.size(), 1);
        QCOMPARE(list.match(u"https://a.example"_s, u"me"_s)->name, u"Renamed"_s);
        QVERIFY(!list.match(u"https://a.example"_s, u"you"_s));
    }

    void renamesAndFallsBackToTheHost()
    {
        ServerList list;
        list.put({u"a"_s, u"music.example.org"_s, u"https://music.example.org"_s, u"me"_s});
        QVERIFY(list.rename(u"a"_s, u"  Home \n server "_s));
        QCOMPARE(list.find(u"a"_s)->name, u"Home server"_s);
        QVERIFY(!list.rename(u"a"_s, u"Home server"_s));
        QVERIFY(list.rename(u"a"_s, u"   "_s));
        QCOMPARE(list.find(u"a"_s)->name, u"music.example.org"_s);
        QVERIFY(!list.rename(u"missing"_s, u"X"_s));
    }

    void namesAServerByItsHost()
    {
        QCOMPARE(ServerList::defaultName(u"https://music.example.org:8443/navidrome"_s), u"music.example.org"_s);
    }
};

QTEST_GUILESS_MAIN(ServerListTest)
#include "serverlist_test.moc"
