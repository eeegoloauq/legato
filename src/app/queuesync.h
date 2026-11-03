// Keeps the play queue on the server (docs/architecture.md, "Queue resume"): savePlayQueue after
// legato's own changes, getPlayQueue after signing in.
//
// Only a change made here marks the queue for saving, so a legato left open and idle never writes
// over a queue another client saved since. Until such a change, the window coming to the front asks
// the server again and takes a newer queue (one saved in another app meanwhile).
#pragma once

#include <QObject>
#include <QTimer>

namespace legato {

class Player;
class SubsonicClient;

class QueueSync : public QObject
{
    Q_OBJECT
public:
    QueueSync(SubsonicClient *client, Player *player, QObject *parent = nullptr);

    // Sends a change not saved yet, to the server the client points at now: before a switch.
    void flush();
    // The client points at another server (or none). Asks it which extensions it has, and puts
    // its saved queue in the player when nothing is there.
    void serverChanged();
    // At quit: flush, then waits up to `ms` for the server to answer.
    void flushAndWait(int ms);

signals:
    // A save finished (or failed).
    void saveFinished();

private:
    void changed(int delayMs);
    void save();
    void restore();
    // The window came to the front.
    void activated();

    SubsonicClient *m_client;
    Player *m_player;
    QTimer m_debounce;
    QTimer m_heartbeat;
    bool m_dirty = false;
    bool m_saving = false;
    // The player is taking a queue from the server: not a change to send back.
    bool m_applying = false;
    // Nothing changed here since the queue came from the server (or there is none): a newer one
    // saved elsewhere may replace it.
    bool m_untouched = false;
    qint64 m_lastCheck = 0;
    int m_generation = 0;
};

} // namespace legato
