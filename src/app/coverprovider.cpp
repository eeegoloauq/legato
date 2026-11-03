#include "coverprovider.h"

#include "coverstore.h"

#include <QBuffer>
#include <QImageReader>
#include <QMutex>

#include <atomic>
#include <memory>

namespace legato {

namespace {

// Shared between the response and the work it started, which may finish after the engine has
// cancelled the response.
struct State {
    std::atomic_bool done = false;
    CoverStore::Cancelled cancelled = std::make_shared<std::atomic_bool>(false);
    QMutex mutex;
    QImage image;
    QString error;
};

class CoverResponse : public QQuickImageResponse
{
public:
    CoverResponse(CoverStore *store, const QString &id, const QSize &requested)
    {
        // Covers are shown at up to three sizes; asking the server for buckets keeps the cache small.
        const int pixels = std::max(requested.width(), requested.height());
        const int bucket = CoverStore::bucketFor(pixels > 0 ? pixels : 600);
        store->load(id, bucket, m_state->cancelled, [this, state = m_state, requested](const QByteArray &bytes, const QString &error) {
            if (state->done)
                return;
            QImage image = error.isEmpty() ? CoverProvider::decode(bytes, requested) : QImage();
            {
                QMutexLocker lock(&state->mutex);
                state->image = std::move(image);
                state->error = !error.isEmpty() ? error : state->image.isNull() ? QStringLiteral("cannot decode cover") : QString();
            }
            // The engine deletes the response only after finished(), so `this` is alive here.
            if (!state->done.exchange(true))
                emit finished();
        });
    }

    QQuickTextureFactory *textureFactory() const override
    {
        QMutexLocker lock(&m_state->mutex);
        return QQuickTextureFactory::textureFactoryForImage(m_state->image);
    }

    QString errorString() const override
    {
        QMutexLocker lock(&m_state->mutex);
        return m_state->error;
    }

    void cancel() override
    {
        *m_state->cancelled = true;
        // A cancelled response must still emit finished() so the engine can delete it.
        if (!m_state->done.exchange(true))
            emit finished();
    }

private:
    std::shared_ptr<State> m_state = std::make_shared<State>();
};

} // namespace

CoverProvider::CoverProvider(CoverStore *store)
    : m_store(store)
{
}

QQuickImageResponse *CoverProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new CoverResponse(m_store, id, requestedSize);
}

QImage CoverProvider::decode(const QByteArray &bytes, const QSize &size)
{
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    if (size.isValid() && full.isValid()) {
        // Scale while decoding (JPEG decodes at a fraction of the cost), then crop the center.
        reader.setScaledSize(full.scaled(size, Qt::KeepAspectRatioByExpanding));
        QImage image = reader.read();
        if (image.isNull() || image.size() == size)
            return image;
        const QRect crop((image.width() - size.width()) / 2, (image.height() - size.height()) / 2,
                         size.width(), size.height());
        return image.copy(crop);
    }
    return reader.read();
}

} // namespace legato
