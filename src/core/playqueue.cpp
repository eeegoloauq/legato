#include "playqueue.h"

#include <QRandomGenerator>

#include <algorithm>
#include <numeric>

namespace legato {

namespace {

QList<int> sequence(int size)
{
    QList<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    return order;
}

// `first`, then every other index of 0..size-1 in random order.
QList<int> shuffledAfter(int first, int size)
{
    QList<int> rest;
    rest.reserve(size);
    for (int i = 0; i < size; ++i) {
        if (i != first)
            rest.append(i);
    }
    std::shuffle(rest.begin(), rest.end(), *QRandomGenerator::global());
    rest.prepend(first);
    return rest;
}

} // namespace

void PlayQueue::replace(QList<QueueEntry> entries, int start, bool shuffle)
{
    m_entries = std::move(entries);
    const int count = int(m_entries.size());
    if (count == 0) {
        m_order.clear();
        m_position = -1;
        return;
    }
    if (start >= count)
        start = 0;
    m_shuffled = shuffle;
    if (shuffle) {
        if (start < 0)
            start = int(QRandomGenerator::global()->bounded(count));
        m_order = shuffledAfter(start, count);
        m_position = 0;
    } else {
        m_order = sequence(count);
        m_position = std::max(start, 0);
    }
}

void PlayQueue::setPosition(int index)
{
    if (index >= 0 && index < size())
        m_position = index;
}

int PlayQueue::following(bool repeatAll) const
{
    if (m_position < 0)
        return -1;
    if (m_position + 1 < size())
        return m_position + 1;
    return repeatAll ? 0 : -1;
}

void PlayQueue::setShuffle(bool on)
{
    if (m_position < 0)
        return;
    const int playing = m_order.at(m_position);
    const int count = int(m_entries.size());
    m_shuffled = on;
    if (on) {
        m_order = shuffledAfter(playing, count);
        m_position = 0;
    } else {
        m_order = sequence(count);
        m_position = playing;
    }
}

void PlayQueue::remove(int index)
{
    if (index < 0 || index >= size() || index == m_position)
        return;
    const int entry = m_order.takeAt(index);
    m_entries.removeAt(entry);
    for (int &i : m_order) {
        if (i > entry)
            --i;
    }
    if (index < m_position)
        --m_position;
}

void PlayQueue::clearUpNext()
{
    while (size() > m_position + 1)
        remove(size() - 1);
}

void PlayQueue::insert(QList<QueueEntry> entries, bool next)
{
    if (entries.isEmpty())
        return;
    if (m_position < 0) {
        replace(std::move(entries), 0, false);
        return;
    }
    const int added = int(entries.size());
    if (!m_shuffled) {
        // The order is the given one: the songs go into the list itself.
        const int at = next ? m_order.at(m_position) + 1 : int(m_entries.size());
        for (int i = 0; i < added; ++i)
            m_entries.insert(at + i, entries.at(i));
        m_order = sequence(int(m_entries.size()));
        return;
    }
    const int first = int(m_entries.size());
    m_entries.append(std::move(entries));
    const int at = next ? m_position + 1 : size();
    for (int i = 0; i < added; ++i)
        m_order.insert(at + i, first + i);
}

bool PlayQueue::setStarred(const QString &songId, bool starred)
{
    bool changed = false;
    for (QueueEntry &entry : m_entries) {
        if (entry.song.id == songId && entry.song.starred != starred) {
            entry.song.starred = starred;
            changed = true;
        }
    }
    return changed;
}

} // namespace legato
