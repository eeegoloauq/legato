// The play queue: the songs as they were given and the order they play in.
//
// legato keeps the order itself instead of handing mpv the whole list. mpv's own shuffle moves the
// playing song too, so it could land last with nothing after it, and its playlist positions say
// nothing about which list they belong to. The player only gives mpv the current song and the
// following one (for gapless playback).
#pragma once

#include "subsonic.h"

#include <QList>
#include <QUrl>

namespace legato {

struct QueueEntry {
    Song song;
    QUrl url;
};

class PlayQueue
{
public:
    // Replaces the songs. A negative start means from the top: the first song, or a random one
    // when shuffling. Shuffling puts the starting song first and the others in random order.
    void replace(QList<QueueEntry> entries, int start, bool shuffle);

    int size() const { return int(m_order.size()); }
    bool isEmpty() const { return m_order.isEmpty(); }
    // In play order. -1 when empty.
    int position() const { return m_position; }
    void setPosition(int index);
    // In play order.
    const QueueEntry &at(int index) const { return m_entries.at(m_order.at(index)); }
    const QueueEntry *current() const { return m_position >= 0 ? &at(m_position) : nullptr; }

    // The index that plays after the current song: the next one, the first with repeat all at the
    // end, or -1.
    int following(bool repeatAll) const;

    // On: the current song first, everything else after it in random order. Off: the given order,
    // at the current song.
    void setShuffle(bool on);
    // Any song but the current one.
    void remove(int index);
    // Drops every song after the current one.
    void clearUpNext();
    // Play Next (`next`) puts the songs right after the current one, Add to Queue at the end of
    // the play order. Unshuffled, they also take that place in the given order, so turning shuffle
    // off keeps them there. An empty queue starts at the first of them.
    void insert(QList<QueueEntry> entries, bool next);
    // Marks every entry of the song. Returns whether any changed.
    bool setStarred(const QString &songId, bool starred);

private:
    QList<QueueEntry> m_entries; // as given
    QList<int> m_order;          // play order, indices into m_entries
    int m_position = -1;
    bool m_shuffled = false;
};

} // namespace legato
