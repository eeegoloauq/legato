// MPRIS 2 (docs/architecture.md, "Desktop integration"): media keys, the GNOME and KDE player
// controls and playerctl reach legato over the session bus. Linux only.
//
// Property changes are collected and sent as one PropertiesChanged per event loop turn, and only
// for values that changed. Position is never sent as a change (the spec forbids it); readers ask
// for it, and Seeked tells them when it jumps.
#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace legato {

class CoverStore;
class MprisPlayer;
class Player;

class Mpris : public QObject
{
    Q_OBJECT
public:
    Mpris(Player *player, CoverStore *covers, QObject *parent = nullptr);
    ~Mpris() override;

    // Whether legato is on the bus. False without a session bus.
    bool registered() const { return !m_service.isEmpty(); }

    Player *player() const { return m_player; }
    QVariantMap metadata() const { return m_metadata; }
    QDBusObjectPath trackId() const;
    QString playbackStatus() const;

signals:
    // A desktop asked to see legato's window.
    void raiseRequested();

private:
    // Sends what changed, once the current event loop turn is over.
    void schedule();
    void flush();
    QVariantMap buildMetadata() const;
    void updateArt();

    Player *m_player;
    CoverStore *m_covers;
    MprisPlayer *m_adaptor;
    QString m_service;
    // What readers were last told, to send only what changed.
    QVariantMap m_props;
    QVariantMap m_metadata;
    QString m_artFor;
    QString m_artUrl;
    // Counts songs, for track ids that differ from one song to the next.
    quint64 m_track = 0;
    bool m_flushQueued = false;
};

// org.mpris.MediaPlayer2
class MprisRoot : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen)
    Q_PROPERTY(bool Fullscreen READ fullscreen)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRoot(Mpris *mpris);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool canSetFullscreen() const { return false; }
    bool fullscreen() const { return false; }
    bool hasTrackList() const { return false; }
    QString identity() const;
    QString desktopEntry() const;
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise();
    void Quit();

private:
    Mpris *m_mpris;
};

// org.mpris.MediaPlayer2.Player
class MprisPlayer : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ rate)
    Q_PROPERTY(double MaximumRate READ rate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ hasSong)
    Q_PROPERTY(bool CanGoPrevious READ hasSong)
    Q_PROPERTY(bool CanPlay READ hasSong)
    Q_PROPERTY(bool CanPause READ hasSong)
    Q_PROPERTY(bool CanSeek READ hasSong)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayer(Mpris *mpris);

    QString playbackStatus() const { return m_mpris->playbackStatus(); }
    QString loopStatus() const;
    void setLoopStatus(const QString &status);
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const;
    void setShuffle(bool on);
    QVariantMap metadata() const { return m_mpris->metadata(); }
    double volume() const;
    void setVolume(double volume);
    qlonglong position() const;
    bool hasSong() const;
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &) {}

signals:
    void Seeked(qlonglong position);

private:
    Mpris *m_mpris;
};

} // namespace legato
