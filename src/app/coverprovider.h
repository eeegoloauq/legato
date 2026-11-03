// Serves image://cover/<cover id> to QML from the CoverStore.
//
// The image is decoded and center-cropped on a worker thread at exactly the size the Image asks for
// (its sourceSize), so the GPU never scales covers and scrolling never decodes on the GUI thread.
#pragma once

#include <QQuickAsyncImageProvider>

namespace legato {

class CoverStore;

class CoverProvider : public QQuickAsyncImageProvider
{
public:
    explicit CoverProvider(CoverStore *store);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    // Decodes `bytes` and crops the center to `size` (or returns the whole image for an invalid size).
    static QImage decode(const QByteArray &bytes, const QSize &size);

private:
    CoverStore *m_store;
};

} // namespace legato
