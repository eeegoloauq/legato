#include "materialowner.h"

#include <QQuickItem>

namespace legato {

void MaterialOwner::componentComplete()
{
    QObject *shadow = parent();
    if (!shadow)
        return;
    // Without a material of the page's own, `material` is the default one.
    auto *material = shadow->property("material").value<QQuickItem *>();
    if (material && !material->parent())
        material->setParent(shadow);
}

} // namespace legato
