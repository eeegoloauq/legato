// The list models the pages show: albums, songs, artists and playlists. Each is a plain list of
// rows from the server; the page that owns it fills it.
#pragma once

#include "subsonic.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QtQmlIntegration>

namespace legato {

// A list model with a `count` QML can bind to. Each model below keeps its rows in a QList and
// changes them through the helpers here, which tell the views. (Not a template base: QML tooling
// could not resolve a model whose base class is one.)
class ListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : count(); }
    int count() const { return size(); }
    // The id of the row's album, song, artist or playlist; empty for a row that is not there.
    Q_INVOKABLE virtual QString idAt(int row) const = 0;

signals:
    void countChanged();

protected:
    virtual int size() const = 0;

    template <typename T>
    void replaceRows(QList<T> &rows, QList<T> items)
    {
        beginResetModel();
        rows = std::move(items);
        endResetModel();
    }

    template <typename T>
    void appendRows(QList<T> &rows, const QList<T> &items)
    {
        if (items.isEmpty())
            return;
        beginInsertRows({}, int(rows.size()), int(rows.size() + items.size() - 1));
        rows.append(items);
        endInsertRows();
    }

    // Replaces the rows with `items`, telling the views only about the rows between what both lists
    // start and end with. Moving on in a queue removes rows at the top; removing or adding songs
    // touches only those rows. The other rows are kept, so their delegates are not built again.
    // Rows are matched by id; a kept row takes the new row's data without a dataChanged.
    template <typename T>
    void syncRows(QList<T> &rows, QList<T> items)
    {
        const qsizetype before = rows.size();
        const qsizetype after = items.size();
        qsizetype head = 0;
        while (head < before && head < after && rows[head].id == items[head].id)
            ++head;
        qsizetype tail = 0;
        while (tail < before - head && tail < after - head
               && rows[before - 1 - tail].id == items[after - 1 - tail].id)
            ++tail;
        const qsizetype removed = before - head - tail;
        const qsizetype added = after - head - tail;
        if (removed > 0) {
            beginRemoveRows({}, int(head), int(head + removed - 1));
            rows.remove(head, removed);
            endRemoveRows();
        }
        if (added > 0) {
            beginInsertRows({}, int(head), int(head + added - 1));
            rows = std::move(items);
            endInsertRows();
        } else {
            rows = std::move(items);
        }
    }

    // Calls `change` on each row whose id is `id`, and reports the rows it changed.
    template <typename T, typename Change>
    void updateRows(QList<T> &rows, const QString &id, int role, Change change)
    {
        for (qsizetype row = 0; row < rows.size(); ++row) {
            if (rows[row].id == id && change(rows[row])) {
                const QModelIndex at = index(int(row));
                emit dataChanged(at, at, {role});
            }
        }
    }
};

// The rows of a model of T, and reading them.
#define LEGATO_LIST_MODEL_ROWS(T)                                                                    \
public:                                                                                            \
    using ListModel::ListModel;                                                                    \
    const QList<T> &items() const { return m_items; }                                              \
    bool has(int row) const { return row >= 0 && row < m_items.size(); }                          \
    const T &at(int row) const { return m_items.at(row); }                                         \
    QString idAt(int row) const override { return has(row) ? m_items.at(row).id : QString(); }    \
    void set(QList<T> items) { replaceRows(m_items, std::move(items)); }                          \
    void append(const QList<T> &items) { appendRows(m_items, items); }                            \
    void sync(QList<T> items) { syncRows(m_items, std::move(items)); }                            \
                                                                                                   \
protected:                                                                                         \
    int size() const override { return int(m_items.size()); }                                      \
                                                                                                   \
private:                                                                                           \
    QList<T> m_items;                                                                              \
                                                                                                   \
public:

class AlbumModel : public ListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Albums come from the pages' data objects")
    LEGATO_LIST_MODEL_ROWS(Album)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ArtistRole,
        ArtistIdRole,
        ArtistsRole,
        CoverArtRole,
        YearRole,
        HueRole,
        SongCountRole,
        PlayCountRole,
        StarredRole,
    };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setStarred(const QString &id, bool starred);
};

class SongModel : public ListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Songs come from the pages' data objects")
    LEGATO_LIST_MODEL_ROWS(Song)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        ArtistIdRole,
        ArtistsRole,
        AlbumRole,
        AlbumIdRole,
        CoverArtRole,
        DurationRole,
        TrackRole,
        DiscRole,
        DiscHeaderRole,
        YearRole,
        QualityRole,
        StarredRole,
        PlayCountRole,
    };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    // An album's songs: rows that start a disc of an album with several get its number as
    // discHeader.
    void setDiscs(bool on) { m_discs = on; }
    void setStarred(const QString &id, bool starred);

private:
    bool m_discs = false;
};

class ArtistModel : public ListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Artists come from the pages' data objects")
    LEGATO_LIST_MODEL_ROWS(Artist)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, CoverArtRole, AlbumCountRole, HueRole, StarredRole };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setStarred(const QString &id, bool starred);
};

class PlaylistModel : public ListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Playlists come from Library.playlists")
    LEGATO_LIST_MODEL_ROWS(Playlist)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CommentRole,
        OwnerRole,
        CoverArtRole,
        SongCountRole,
        HueRole,
        // The user's own: songs can be added and removed, and it can be renamed and deleted.
        EditableRole,
    };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

// The current song's lyrics (Player.lyrics): a row per line, with its time (seconds, -1 when not
// synced) and lyric (its text).
class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Lyrics come from Player.lyrics")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { TimeRole = Qt::UserRole + 1, TextRole };

    explicit LyricsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : count(); }
    int count() const { return int(m_lines.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void set(const Lyrics &lyrics);
    void clear();
    // The line sung at `seconds`: the last one that has started. -1 before the first and for lyrics
    // that are not synced.
    int lineAt(double seconds) const;

signals:
    void countChanged();

private:
    QList<LyricLine> m_lines;
    bool m_synced = false;
};

// The songs of a SongModel whose title, artists or album hold every word of `text`, ignoring case
// and accents: the search's first group on an album, a playlist or Favorites (docs/design.md,
// "Search"). sourceRow() maps a match back, so it plays on into the whole list.
class SongFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(legato::SongModel *songs READ songs WRITE setSongs NOTIFY songsChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    // True while `text` holds a word: the view shows matches only.
    Q_PROPERTY(bool active READ active NOTIFY textChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit SongFilter(QObject *parent = nullptr);

    SongModel *songs() const { return m_songs; }
    void setSongs(SongModel *songs);
    QString text() const { return m_text; }
    void setText(const QString &text);
    bool active() const { return !m_words.isEmpty(); }
    int count() const { return rowCount(); }

    // The row in `songs` of the row shown here; -1 for none.
    Q_INVOKABLE int sourceRow(int row) const;
    // Up to `count` matches as maps of row (in `songs`), songId, title, artist and coverArt.
    Q_INVOKABLE QVariantList firstMatches(int count) const;
    QVariant data(const QModelIndex &index, int role) const override;

    // Lower case without accents or other marks, for matching: "Beyoncé" -> "beyonce".
    static QString fold(const QString &text);

signals:
    void songsChanged();
    void textChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QPointer<SongModel> m_songs;
    QString m_text;
    QStringList m_words;
};

// A stable placeholder hue for an id, so placeholders differ but never change.
int hueOf(const QString &id);
// Artists as QML reads them: a list of {id, name}.
QVariantList artistList(const QList<ArtistRef> &artists);

} // namespace legato
