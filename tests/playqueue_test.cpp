#include "playqueue.h"

#include <QSet>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

QList<QueueEntry> songs(int count)
{
    QList<QueueEntry> entries;
    for (int i = 0; i < count; ++i) {
        Song song;
        song.id = QString::number(i);
        entries.append({song, QUrl(u"https://example.org/%1"_s.arg(i))});
    }
    return entries;
}

QStringList ids(const PlayQueue &queue)
{
    QStringList out;
    for (int i = 0; i < queue.size(); ++i)
        out.append(queue.at(i).song.id);
    return out;
}

} // namespace

class PlayQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void playsInOrderFromTheChosenSong()
    {
        PlayQueue queue;
        queue.replace(songs(5), 2, false);
        QCOMPARE(queue.position(), 2);
        QCOMPARE(queue.current()->song.id, u"2"_s);
        QCOMPARE(queue.following(false), 3);
    }

    // The bug: mpv's shuffle moved the playing song, sometimes to the end, so nothing followed it.
    void shufflePutsTheChosenSongFirstAndEveryOtherAfterIt()
    {
        for (int run = 0; run < 50; ++run) {
            PlayQueue queue;
            queue.replace(songs(8), 5, true);
            QCOMPARE(queue.position(), 0);
            QCOMPARE(queue.current()->song.id, u"5"_s);
            QCOMPARE(queue.following(false), 1);
            const QStringList order = ids(queue);
            QCOMPARE(QSet<QString>(order.begin(), order.end()).size(), 8);
        }
    }

    void playingAnAlbumShuffledStartsAnywhere()
    {
        QSet<QString> firsts;
        for (int run = 0; run < 200; ++run) {
            PlayQueue queue;
            queue.replace(songs(4), -1, true);
            QCOMPARE(queue.position(), 0);
            firsts.insert(queue.current()->song.id);
        }
        QCOMPARE(firsts.size(), 4);
    }

    void shuffleOffReturnsToTheAlbumOrderAtTheSameSong()
    {
        PlayQueue queue;
        queue.replace(songs(6), 0, false);
        queue.setPosition(3);
        queue.setShuffle(true);
        QCOMPARE(queue.current()->song.id, u"3"_s);
        QCOMPARE(queue.position(), 0);
        queue.setPosition(2);
        const QString playing = queue.current()->song.id;
        queue.setShuffle(false);
        QCOMPARE(ids(queue), QStringList({u"0"_s, u"1"_s, u"2"_s, u"3"_s, u"4"_s, u"5"_s}));
        QCOMPARE(queue.current()->song.id, playing);
    }

    void theEndWrapsOnlyWithRepeatAll()
    {
        PlayQueue queue;
        queue.replace(songs(3), 2, false);
        QCOMPARE(queue.following(false), -1);
        QCOMPARE(queue.following(true), 0);
    }

    void removingKeepsTheCurrentSong()
    {
        PlayQueue queue;
        queue.replace(songs(5), 2, false);
        queue.remove(2);
        QCOMPARE(queue.size(), 5);
        queue.remove(0);
        QCOMPARE(queue.current()->song.id, u"2"_s);
        QCOMPARE(queue.position(), 1);
        queue.remove(2);
        QCOMPARE(ids(queue), QStringList({u"1"_s, u"2"_s, u"4"_s}));
        // A removed song stays removed when shuffle is turned off.
        queue.setShuffle(true);
        queue.setShuffle(false);
        QCOMPARE(ids(queue), QStringList({u"1"_s, u"2"_s, u"4"_s}));
        QCOMPARE(queue.current()->song.id, u"2"_s);
    }

    void clearingUpNextKeepsWhatPlayed()
    {
        PlayQueue queue;
        queue.replace(songs(5), 1, false);
        queue.clearUpNext();
        QCOMPARE(ids(queue), QStringList({u"0"_s, u"1"_s}));
        QCOMPARE(queue.following(false), -1);
    }

    void playNextAndAddToQueueInOrder()
    {
        PlayQueue queue;
        queue.replace(songs(3), 1, false);
        QList<QueueEntry> extra = songs(2);
        extra[0].song.id = u"a"_s;
        extra[1].song.id = u"b"_s;
        queue.insert(extra, true);
        QCOMPARE(ids(queue), QStringList({u"0"_s, u"1"_s, u"a"_s, u"b"_s, u"2"_s}));
        QCOMPARE(queue.current()->song.id, u"1"_s);
        queue.insert({extra[0]}, false);
        QCOMPARE(ids(queue).last(), u"a"_s);
        // Unshuffled, they are part of the list: shuffle on and off keeps them in place.
        queue.setShuffle(true);
        queue.setShuffle(false);
        QCOMPARE(ids(queue), QStringList({u"0"_s, u"1"_s, u"a"_s, u"b"_s, u"2"_s, u"a"_s}));
    }

    void playNextWhileShuffledComesRightAfter()
    {
        PlayQueue queue;
        queue.replace(songs(6), 3, true);
        QList<QueueEntry> extra = songs(1);
        extra[0].song.id = u"x"_s;
        queue.insert(extra, true);
        QCOMPARE(queue.size(), 7);
        QCOMPARE(queue.at(queue.following(false)).song.id, u"x"_s);
        queue.insert(extra, false);
        QCOMPARE(queue.at(queue.size() - 1).song.id, u"x"_s);
    }

    void queueingIntoAnEmptyQueueStartsIt()
    {
        PlayQueue queue;
        queue.insert(songs(2), true);
        QCOMPARE(queue.position(), 0);
        QCOMPARE(queue.current()->song.id, u"0"_s);
    }

    void starringMarksEveryCopy()
    {
        QList<QueueEntry> entries = songs(3);
        entries[2].song.id = u"0"_s;
        PlayQueue queue;
        queue.replace(entries, 0, false);
        QVERIFY(queue.setStarred(u"0"_s, true));
        QVERIFY(queue.at(0).song.starred);
        QVERIFY(queue.at(2).song.starred);
        QVERIFY(!queue.setStarred(u"0"_s, true));
    }
};

QTEST_GUILESS_MAIN(PlayQueueTest)
#include "playqueue_test.moc"
