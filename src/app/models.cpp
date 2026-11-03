#include "models.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace legato {

int hueOf(const QString &id)
{
    // FNV-1a.
    quint32 hash = 2166136261u;
    for (QChar c : id) {
        hash ^= c.unicode();
        hash *= 16777619u;
    }
    return int(hash % 360);
}

QVariantList artistList(const QList<ArtistRef> &artists)
{
    QVariantList out;
    out.reserve(artists.size());
    for (const ArtistRef &artist : artists)
        out.append(QVariantMap{{u"id"_s, artist.id}, {u"name"_s, artist.name}});
    return out;
}

ListModel::ListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &ListModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ListModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ListModel::countChanged);
}

QVariant AlbumModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Album &album = at(index.row());
    switch (role) {
    case IdRole:
        return album.id;
    case NameRole:
        return album.name;
    case ArtistRole:
        return album.artist;
    case ArtistIdRole:
        return album.artistId;
    case ArtistsRole:
        return artistList(album.artists);
    case CoverArtRole:
        return album.coverArt;
    case YearRole:
        return album.year;
    case HueRole:
        return hueOf(album.id);
    case SongCountRole:
        return album.songCount;
    case PlayCountRole:
        return album.playCount;
    case StarredRole:
        return album.starred;
    }
    return {};
}

QHash<int, QByteArray> AlbumModel::roleNames() const
{
    return {{IdRole, "albumId"},   {NameRole, "name"},           {ArtistRole, "artist"},
            {ArtistIdRole, "artistId"}, {ArtistsRole, "artists"}, {CoverArtRole, "coverArt"},
            {YearRole, "year"},    {HueRole, "hue"},             {SongCountRole, "songCount"},
            {PlayCountRole, "playCount"}, {StarredRole, "starred"}};
}

void AlbumModel::setStarred(const QString &id, bool starred)
{
    updateRows(m_items, id, StarredRole, [starred](Album &album) { return std::exchange(album.starred, starred) != starred; });
}

QVariant SongModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const int row = index.row();
    const Song &song = at(row);
    switch (role) {
    case IdRole:
        return song.id;
    case TitleRole:
        return song.title;
    case ArtistRole:
        return song.artist;
    case ArtistIdRole:
        return song.artistId;
    case ArtistsRole:
        return artistList(song.artists);
    case AlbumRole:
        return song.album;
    case AlbumIdRole:
        return song.albumId;
    case CoverArtRole:
        return song.coverArt;
    case DurationRole:
        return song.duration;
    case TrackRole:
        return song.track;
    case DiscRole:
        return song.disc;
    case DiscHeaderRole: {
        if (!m_discs || m_items.constFirst().disc == m_items.constLast().disc)
            return 0;
        return row == 0 || at(row - 1).disc != song.disc ? song.disc : 0;
    }
    case YearRole:
        return song.year;
    case QualityRole:
        return subsonic::qualityLabel(song);
    case StarredRole:
        return song.starred;
    case PlayCountRole:
        return song.playCount;
    }
    return {};
}

QHash<int, QByteArray> SongModel::roleNames() const
{
    return {{IdRole, "songId"},      {TitleRole, "title"},       {ArtistRole, "artist"},
            {ArtistIdRole, "artistId"}, {ArtistsRole, "artists"}, {AlbumRole, "album"},
            {AlbumIdRole, "albumId"}, {CoverArtRole, "coverArt"}, {DurationRole, "duration"},
            {TrackRole, "track"},    {DiscRole, "disc"},         {DiscHeaderRole, "discHeader"},
            {YearRole, "year"},      {QualityRole, "quality"},   {StarredRole, "starred"},
            {PlayCountRole, "playCount"}};
}

void SongModel::setStarred(const QString &id, bool starred)
{
    updateRows(m_items, id, StarredRole, [starred](Song &song) { return std::exchange(song.starred, starred) != starred; });
}

QVariant ArtistModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Artist &artist = at(index.row());
    switch (role) {
    case IdRole:
        return artist.id;
    case NameRole:
        return artist.name;
    case CoverArtRole:
        return artist.coverArt;
    case AlbumCountRole:
        return artist.albumCount;
    case HueRole:
        return hueOf(artist.id);
    case StarredRole:
        return artist.starred;
    }
    return {};
}

QHash<int, QByteArray> ArtistModel::roleNames() const
{
    return {{IdRole, "artistId"}, {NameRole, "name"}, {CoverArtRole, "coverArt"},
            {AlbumCountRole, "albumCount"}, {HueRole, "hue"}, {StarredRole, "starred"}};
}

void ArtistModel::setStarred(const QString &id, bool starred)
{
    updateRows(m_items, id, StarredRole, [starred](Artist &artist) { return std::exchange(artist.starred, starred) != starred; });
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Playlist &playlist = at(index.row());
    switch (role) {
    case IdRole:
        return playlist.id;
    case NameRole:
        return playlist.name;
    case CommentRole:
        return playlist.comment;
    case OwnerRole:
        return playlist.owner;
    case CoverArtRole:
        return playlist.coverArt;
    case SongCountRole:
        return playlist.songCount;
    case HueRole:
        return hueOf(playlist.id);
    }
    return {};
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {{IdRole, "playlistId"}, {NameRole, "name"}, {CommentRole, "comment"}, {OwnerRole, "owner"},
            {CoverArtRole, "coverArt"}, {SongCountRole, "songCount"}, {HueRole, "hue"}};
}

// SongFilter

SongFilter::SongFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
        connect(this, signal, this, &SongFilter::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SongFilter::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SongFilter::countChanged);
}

void SongFilter::setSongs(SongModel *songs)
{
    if (songs == m_songs)
        return;
    m_songs = songs;
    setSourceModel(songs);
    emit songsChanged();
}

void SongFilter::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    const QStringList words = fold(text).split(u' ', Qt::SkipEmptyParts);
    emit textChanged();
    if (words == m_words)
        return;
    const bool wasActive = active();
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_words = words;
    endFilterChange(Direction::Rows);
#else
    m_words = words;
    invalidateRowsFilter();
#endif
    // Rows kept through the change still hold the disc headers of before.
    if (active() != wasActive && rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {SongModel::DiscHeaderRole});
}

int SongFilter::sourceRow(int row) const
{
    return mapToSource(index(row, 0)).row();
}

QVariant SongFilter::data(const QModelIndex &index, int role) const
{
    // Disc headers mark where a disc starts in the whole album; among matches they would mislead.
    if (role == SongModel::DiscHeaderRole && active())
        return 0;
    return QSortFilterProxyModel::data(index, role);
}

QString SongFilter::fold(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining
            || c.category() == QChar::Mark_Enclosing)
            continue;
        out.append(c.isSpace() ? u' ' : c);
    }
    return out.toCaseFolded();
}

bool SongFilter::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (m_words.isEmpty())
        return true;
    if (!m_songs || !m_songs->has(sourceRow))
        return false;
    const Song &song = m_songs->at(sourceRow);
    QString haystack = song.title + u' ' + song.artist + u' ' + song.album;
    for (const ArtistRef &artist : song.artists)
        haystack += u' ' + artist.name;
    const QString folded = fold(haystack);
    return std::all_of(m_words.begin(), m_words.end(),
                       [&](const QString &word) { return folded.contains(word); });
}

} // namespace legato
