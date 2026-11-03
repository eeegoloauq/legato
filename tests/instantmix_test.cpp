#include "instantmix.h"

#include <QRandomGenerator>
#include <QSet>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace legato;

namespace {

Song song(const QString &id, const QString &artistId)
{
    Song s;
    s.id = id;
    s.artistId = artistId;
    s.artist = u"Artist "_s + artistId;
    return s;
}

// `counts` songs for artists a, b, c, ...
QList<Song> songsBy(const QList<int> &counts)
{
    QList<Song> out;
    for (int artist = 0; artist < counts.size(); ++artist) {
        const QString key = QString(QChar(u'a' + artist));
        for (int n = 0; n < counts.at(artist); ++n)
            out.append(song(key + QString::number(n), key));
    }
    return out;
}

int sideBySide(const QList<Song> &mix)
{
    int pairs = 0;
    for (qsizetype i = 1; i < mix.size(); ++i)
        pairs += mix.at(i).artistId == mix.at(i - 1).artistId;
    return pairs;
}

} // namespace

class InstantMixTest : public QObject
{
    Q_OBJECT

private slots:
    void seedPlaysFirstOnceAndRepeatsAreDropped()
    {
        QList<Song> similar = songsBy({3, 2});
        similar.append(similar.at(1));
        const Song seed = song(u"seed"_s, u"a"_s);
        similar.insert(2, seed);
        QRandomGenerator random(7);
        const QList<Song> mix = arrangeMix(similar, seed, random);
        QCOMPARE(mix.size(), 6);
        QCOMPARE(mix.first().id, u"seed"_s);
        QSet<QString> ids;
        for (const Song &s : mix)
            ids.insert(s.id);
        QCOMPARE(ids.size(), 6);
    }

    void withoutSeedKeepsEverySong()
    {
        QRandomGenerator random(1);
        QCOMPARE(arrangeMix(songsBy({4, 4, 2}), std::nullopt, random).size(), 10);
        QVERIFY(arrangeMix({}, std::nullopt, random).isEmpty());
    }

    // As the server sends it for one artist: a third theirs, the rest spread over others.
    void noArtistTwiceInARowWhenAvoidable()
    {
        for (quint32 seed = 0; seed < 200; ++seed) {
            QRandomGenerator random(seed);
            QCOMPARE(sideBySide(arrangeMix(songsBy({15, 12, 7, 5, 5, 3, 2, 1}), std::nullopt, random)), 0);
            // Exactly half: only strict alternation works.
            QCOMPARE(sideBySide(arrangeMix(songsBy({5, 3, 2}), std::nullopt, random)), 0);
            // The seed's artist does not follow the seed.
            const QList<Song> mix = arrangeMix(songsBy({3, 3}), song(u"s"_s, u"a"_s), random);
            QCOMPARE(sideBySide(mix), 0);
        }
    }

    void oneArtistTooManyIsSpreadAsFarAsItGoes()
    {
        QRandomGenerator random(3);
        // 8 of a and 2 others: a needs 7 songs between, there are 2, so 5 pairs are unavoidable.
        QCOMPARE(sideBySide(arrangeMix(songsBy({8, 1, 1}), std::nullopt, random)), 5);
    }

    void orderChangesFromMixToMix()
    {
        QRandomGenerator first(1);
        QRandomGenerator second(2);
        const QList<Song> similar = songsBy({6, 6, 6});
        QStringList a;
        QStringList b;
        for (const Song &s : arrangeMix(similar, std::nullopt, first))
            a.append(s.id);
        for (const Song &s : arrangeMix(similar, std::nullopt, second))
            b.append(s.id);
        QVERIFY(a != b);
    }
};

QTEST_GUILESS_MAIN(InstantMixTest)
#include "instantmix_test.moc"
