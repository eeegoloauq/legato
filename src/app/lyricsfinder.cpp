#include "lyricsfinder.h"

#include "diskcache.h"
#include "lrclib.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>

using namespace Qt::Literals::StringLiterals;

Q_LOGGING_CATEGORY(lcLrclib, "legato.lrclib")

namespace legato {

namespace {

constexpr int TimeoutMs = 15000;

} // namespace

LyricsFinder::LyricsFinder(QNetworkAccessManager *network, QString cacheDirectory, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_directory(std::move(cacheDirectory))
{
}

QString LyricsFinder::cachePath(const Song &song) const
{
    // By what LRCLIB is asked, not the server's id: the same song on another server is the same answer.
    const QString key = QStringList{song.artist, song.title, song.album, QString::number(song.duration)}
                            .join(u'\n')
                            .toLower();
    return m_directory + u'/' + QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex() + ".json"_L1;
}

void LyricsFinder::remember(const Song &song, const QJsonObject &entry) const
{
    QDir().mkpath(m_directory);
    QSaveFile file(cachePath(song));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

QNetworkReply *LyricsFinder::get(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(TimeoutMs);
    // LRCLIB asks clients to name themselves.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"Legato/%1 (Subsonic music player)"_s.arg(QCoreApplication::applicationVersion()));
    return m_network->get(request);
}

void LyricsFinder::find(const Song &song, std::function<void(const Lyrics &)> done)
{
    if (song.title.isEmpty() || song.duration <= 0) {
        done({});
        return;
    }

    if (QFile file(cachePath(song)); file.open(QIODevice::ReadOnly)) {
        diskcache::markUsed(file.fileName());
        const QJsonObject entry = QJsonDocument::fromJson(file.readAll()).object();
        if (entry.contains("record"_L1)) {
            done(lrclib::parseRecord(entry.value("record"_L1).toObject()).lyrics);
            return;
        }
        const qint64 checked = entry.value("none"_L1).toInteger();
        if (checked > 0 && QDateTime::currentSecsSinceEpoch() - checked < qint64(NoneDays) * 86400) {
            done({});
            return;
        }
    }

    // Found (or instrumental): kept for good. Not there: kept as "none" for a while.
    auto answer = [this, song, done](const QJsonObject &record) {
        if (record.isEmpty()) {
            qCInfo(lcLrclib) << "none:" << song.artist << "-" << song.title;
            remember(song, {{u"none"_s, QDateTime::currentSecsSinceEpoch()}});
            done({});
            return;
        }
        const lrclib::Result result = lrclib::parseRecord(record);
        qCInfo(lcLrclib) << (result.answer == lrclib::Answer::Instrumental ? "instrumental:"
                             : result.lyrics.synced                         ? "synced:"
                                                                            : "plain:")
                         << song.artist << "-" << song.title;
        remember(song, {{u"record"_s, record}});
        done(result.lyrics);
    };
    auto failed = [song, done](QNetworkReply *reply) {
        qCInfo(lcLrclib) << "no answer:" << song.artist << "-" << song.title << reply->errorString();
        done({});
    };

    QNetworkReply *reply = get(lrclib::getUrl(song));
    connect(reply, &QNetworkReply::finished, this, [this, reply, song, answer, failed] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            const QJsonObject record = QJsonDocument::fromJson(reply->readAll()).object();
            if (lrclib::parseRecord(record).answer != lrclib::Answer::None) {
                answer(record);
                return;
            }
        } else if (status != 404) {
            failed(reply);
            return;
        }
        // An exact match missed (often the album is named differently): search by title and artist,
        // and take a song of the same length.
        QNetworkReply *search = get(lrclib::searchUrl(song));
        connect(search, &QNetworkReply::finished, this, [search, song, answer, failed] {
            search->deleteLater();
            if (search->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                failed(search);
                return;
            }
            answer(lrclib::pickSearchRecord(QJsonDocument::fromJson(search->readAll()).array(), song.duration));
        });
    });
}

} // namespace legato
