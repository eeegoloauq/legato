#include "models.h"
#include "pages.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

Song song(const QString &id, int disc, int seconds = 60, qint64 size = 0)
{
    Song s;
    s.id = id;
    s.disc = disc;
    s.duration = seconds;
    s.size = size;
    s.suffix = u"flac"_s;
    s.bitDepth = 16;
    s.samplingRate = 44100;
    return s;
}

} // namespace

class ModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void marksWhereEachDiscStarts()
    {
        SongModel model;
        model.setDiscs(true);
        model.set({song(u"1"_s, 1), song(u"2"_s, 1), song(u"3"_s, 2)});
        const auto header = [&](int row) { return model.data(model.index(row), SongModel::DiscHeaderRole).toInt(); };
        QCOMPARE(header(0), 1);
        QCOMPARE(header(1), 0);
        QCOMPARE(header(2), 2);
        // One disc: no headers at all.
        model.set({song(u"1"_s, 1), song(u"2"_s, 1)});
        QCOMPARE(header(0), 0);
    }

    void countFollowsTheRows()
    {
        AlbumModel model;
        QSignalSpy spy(&model, &ListModel::countChanged);
        model.set({Album{u"a"_s}});
        model.append({Album{u"b"_s}, Album{u"c"_s}});
        QCOMPARE(model.count(), 3);
        QCOMPARE(spy.count(), 2);
    }

    // Up Next: a song change removes rows at the top instead of resetting the list, so the rows
    // on screen are not built again (the reset made each song change stall the interface).
    void syncTouchesOnlyTheRowsThatChanged()
    {
        const auto songs = [](const QStringList &ids) {
            QList<Song> out;
            for (const QString &id : ids)
                out.append(song(id, 1));
            return out;
        };
        const auto ids = [](const SongModel &model) {
            QStringList out;
            for (const Song &s : model.items())
                out.append(s.id);
            return out;
        };
        SongModel model;
        model.set(songs({u"a"_s, u"b"_s, u"c"_s, u"d"_s, u"e"_s}));
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

        // Jumping two songs ahead.
        model.sync(songs({u"c"_s, u"d"_s, u"e"_s}));
        QCOMPARE(removed.size(), 1);
        QCOMPARE(removed.at(0).at(1).toInt(), 0);
        QCOMPARE(removed.at(0).at(2).toInt(), 1);
        QVERIFY(inserted.isEmpty());

        // Play Next: a song at the top.
        model.sync(songs({u"x"_s, u"c"_s, u"d"_s, u"e"_s}));
        QCOMPARE(inserted.size(), 1);
        QCOMPARE(inserted.at(0).at(1).toInt(), 0);
        QCOMPARE(inserted.at(0).at(2).toInt(), 0);

        // Removing one in the middle.
        model.sync(songs({u"x"_s, u"c"_s, u"e"_s}));
        QCOMPARE(removed.size(), 2);
        QCOMPARE(removed.at(1).at(1).toInt(), 2);
        QCOMPARE(removed.at(1).at(2).toInt(), 2);

        // Nothing new: nothing said.
        model.sync(songs({u"x"_s, u"c"_s, u"e"_s}));
        QCOMPARE(removed.size(), 2);
        QCOMPARE(inserted.size(), 1);

        // Another order altogether still ends with the right rows.
        model.sync(songs({u"e"_s, u"x"_s, u"c"_s}));
        QCOMPARE(ids(model), (QStringList{u"e"_s, u"x"_s, u"c"_s}));
        QCOMPARE(model.count(), 3);
        QVERIFY(resets.isEmpty());
    }

    void starsEveryRowOfTheId()
    {
        SongModel model;
        model.set({song(u"1"_s, 1), song(u"2"_s, 1), song(u"1"_s, 1)});
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
        model.setStarred(u"1"_s, true);
        QCOMPARE(spy.count(), 2);
        QVERIFY(model.data(model.index(2), SongModel::StarredRole).toBool());
        model.setStarred(u"1"_s, true);
        QCOMPARE(spy.count(), 2);
    }

    void tellsSinglesFromAlbums()
    {
        Album album;
        album.releaseTypes = {u"ep"_s};
        QVERIFY(isSingleOrEp(album));
        // A compilation tagged both ways is an album.
        album.releaseTypes = {u"album"_s, u"single"_s};
        QVERIFY(!isSingleOrEp(album));
        // Without release types, the song count decides.
        album.releaseTypes = {};
        album.songCount = 2;
        QVERIFY(isSingleOrEp(album));
        album.songCount = 9;
        QVERIFY(!isSingleOrEp(album));
    }

    void summarizesSongs()
    {
        const QList<Song> songs = {song(u"1"_s, 1, 1200, 200'000'000), song(u"2"_s, 1, 1260, 112'000'000)};
        QCOMPARE(songSummary(songs, true), u"2 songs, 41 minutes, 312 MB · FLAC · 16-bit / 44.1 kHz"_s);
        QCOMPARE(songSummary({song(u"1"_s, 1, 3 * 3600 + 5 * 60)}, false), u"1 song, 3 hours, 5 minutes"_s);
        QList<Song> mixed = songs;
        mixed[1].suffix = u"mp3"_s;
        // Mixed formats: none named.
        QCOMPARE(songSummary(mixed, true), u"2 songs, 41 minutes, 312 MB"_s);
    }

    // Find in page: every word somewhere in title, artists or album, without case or accents.
    void filterMatchesEveryWordAndMapsBack()
    {
        const auto make = [](const QString &id, const QString &title, const QString &artist, int disc) {
            Song s = song(id, disc);
            s.title = title;
            s.artist = artist;
            s.album = u"Lemonade"_s;
            return s;
        };
        SongModel model;
        model.setDiscs(true);
        model.set({make(u"1"_s, u"Pray You Catch Me"_s, u"Beyoncé"_s, 1),
                   make(u"2"_s, u"Hold Up"_s, u"Beyoncé"_s, 1),
                   make(u"3"_s, u"Freedom"_s, u"Beyoncé feat. Kendrick Lamar"_s, 2)});
        SongFilter filter;
        filter.setSongs(&model);
        QSignalSpy counted(&filter, &SongFilter::countChanged);
        QCOMPARE(filter.count(), 3);
        QVERIFY(!filter.active());

        QSignalSpy changed(&filter, &QAbstractItemModel::dataChanged);
        filter.setText(u"  BEYONCE  kendrick "_s);
        // The row a view kept learns that its disc header is gone.
        QCOMPARE(changed.count(), 1);
        QVERIFY(filter.active());
        QCOMPARE(filter.count(), 1);
        QCOMPARE(filter.sourceRow(0), 2);
        QVERIFY(counted.count() > 0);
        // Disc 2 starts at this row in the album, not among the matches.
        QCOMPARE(filter.data(filter.index(0, 0), SongModel::DiscHeaderRole).toInt(), 0);

        // The search's first group: the rows of `songs`, so a match plays on into the whole list.
        const QVariantList first = filter.firstMatches(5);
        QCOMPARE(first.size(), 1);
        QCOMPARE(first.first().toMap().value(u"row"_s).toInt(), 2);
        QCOMPARE(first.first().toMap().value(u"title"_s).toString(), u"Freedom"_s);

        filter.setText(u"hold"_s);
        QCOMPARE(filter.count(), 1);
        QCOMPARE(filter.sourceRow(0), 1);
        filter.setText(u"lemonade"_s);
        QCOMPARE(filter.count(), 3);
        QCOMPARE(filter.firstMatches(2).size(), 2);
        filter.setText(u"nothing"_s);
        QCOMPARE(filter.count(), 0);
        QCOMPARE(filter.sourceRow(0), -1);

        filter.setText(QString());
        QCOMPARE(filter.count(), 3);
        // Nothing typed, nothing found: the search lists no songs of the page.
        QVERIFY(filter.firstMatches(5).isEmpty());
        QCOMPARE(filter.data(filter.index(2, 0), SongModel::DiscHeaderRole).toInt(), 2);
        QCOMPARE(SongFilter::fold(u"Sigur Rós Ágætis"_s), u"sigur ros agætis"_s);
    }
};

QTEST_GUILESS_MAIN(ModelsTest)
#include "models_test.moc"
