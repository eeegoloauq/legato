// Lyrics from LRCLIB for songs the server has none for (docs/architecture.md, "Lyrics"). Each answer
// is kept on disk, so a song is asked about once: found lyrics and "instrumental" for good, "none"
// for a month (LRCLIB grows). A failed request (LRCLIB is often busy) is not kept, so the song is
// asked again the next time it plays.
#pragma once

#include "subsonic.h"

#include <QJsonObject>
#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace legato {

class LyricsFinder : public QObject
{
    Q_OBJECT

public:
    LyricsFinder(QNetworkAccessManager *network, QString cacheDirectory, QObject *parent = nullptr);

    // `done` gets the lyrics, or none: LRCLIB has none, the song is instrumental, or it did not answer.
    void find(const Song &song, std::function<void(const Lyrics &)> done);

    // Days a "none" is believed.
    static constexpr int NoneDays = 30;

private:
    QNetworkReply *get(const QUrl &url);
    QString cachePath(const Song &song) const;
    void remember(const Song &song, const QJsonObject &entry) const;

    QNetworkAccessManager *m_network;
    QString m_directory;
};

} // namespace legato
