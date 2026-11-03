// The colors of a cover, for tinting the top of an album or artist page (docs/design.md, "Album
// page"): `CoverColors { coverArt: details.coverArt }`.
#pragma once

#include <QColor>
#include <QObject>
#include <QQmlParserStatus>
#include <QVariantList>
#include <QtQmlIntegration>

namespace legato {

class CoverStore;

class CoverColors : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString coverArt READ coverArt WRITE setCoverArt NOTIFY coverArtChanged)
    // Three colors, most prominent first; empty until the cover is read.
    Q_PROPERTY(QVariantList colors READ colors NOTIFY colorsChanged)

public:
    using QObject::QObject;

    void classBegin() override {}
    void componentComplete() override;

    QString coverArt() const { return m_coverArt; }
    void setCoverArt(const QString &coverArt);
    QVariantList colors() const { return m_colors; }

signals:
    void coverArtChanged();
    void colorsChanged();

private:
    void load();

    CoverStore *m_covers = nullptr;
    QString m_coverArt;
    QVariantList m_colors;
};

} // namespace legato
