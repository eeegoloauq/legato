// Gives a RectangularShadow its own default material, for qml/Shadow.qml.
//
// Qt's RectangularShadow (6.12, and its dev branch on 2026-11-06) makes its default material, a
// ShaderEffect, in its private's constructor, before QObject has set the pointer back to the item:
// the material has no owner. The item later shows it as a child item but never deletes it, so every
// shadow destroyed leaves its material and shaders behind, held by the window. One pass through the
// album grid left 176 of them (860 KB). Declared inside the shadow, this makes the shadow the
// material's owner, so it goes with the shadow. A Qt that owns the material is left as it is.
#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QtQmlIntegration>

namespace legato {

class MaterialOwner : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

public:
    using QObject::QObject;

    void classBegin() override {}
    void componentComplete() override;
};

} // namespace legato
