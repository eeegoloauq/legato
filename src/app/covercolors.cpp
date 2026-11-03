#include "covercolors.h"

#include "coverprovider.h"
#include "coverstore.h"
#include "library.h"
#include "palette.h"

#include <QPointer>
#include <QQmlEngine>

namespace legato {

void CoverColors::componentComplete()
{
    m_covers = qmlEngine(this)->singletonInstance<Library *>("Legato", "Library")->covers();
    load();
}

void CoverColors::setCoverArt(const QString &coverArt)
{
    if (coverArt == m_coverArt)
        return;
    m_coverArt = coverArt;
    emit coverArtChanged();
    load();
}

void CoverColors::load()
{
    if (!m_covers)
        return;
    if (!m_colors.isEmpty()) {
        m_colors.clear();
        emit colorsChanged();
    }
    if (m_coverArt.isEmpty())
        return;
    // The size the player reads its palette from, so both share the cached file.
    QPointer<CoverColors> self(this);
    m_covers->load(m_coverArt, 300, nullptr, [self, cover = m_coverArt](const QByteArray &bytes, const QString &error) {
        if (!error.isEmpty())
            return;
        // On a pool thread: decoding stays off the GUI thread.
        const QList<QColor> colors = extractPalette(CoverProvider::decode(bytes, QSize(64, 64)));
        QMetaObject::invokeMethod(self, [self, cover, colors] {
            if (!self || self->m_coverArt != cover)
                return;
            self->m_colors.clear();
            for (const QColor &color : colors)
                self->m_colors.append(color);
            emit self->colorsChanged();
        });
    });
}

} // namespace legato
