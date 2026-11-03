#include "coverprovider.h"
#include "library.h"
#include "player.h"
#include "session.h"
#include "subsonic.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThreadPool>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <clocale>
#include <memory>

#ifdef Q_OS_LINUX
#include "mpris.h"
#endif

Q_IMPORT_QML_PLUGIN(LegatoPlugin)

using namespace Qt::Literals::StringLiterals;

namespace {

// Development options for checking the interface without a display: screenshots, scrolling, test runs.
void applyDevOptions(const QCommandLineParser &args, QQmlApplicationEngine &engine)
{
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return;
    if (args.isSet(u"scroll"_s)) {
        const qreal y = args.value(u"scroll"_s).toDouble();
        QTimer::singleShot(1500, window, [window, y] {
            if (auto *grid = window->findChild<QQuickItem *>(u"albumGrid"_s))
                grid->setProperty("contentY", grid->property("originY").toReal() + y);
        });
    }
    if (args.isSet(u"run-test"_s)) {
        const QString test = args.value(u"run-test"_s);
        // After the covers of the first rows have arrived.
        QTimer::singleShot(4000, window, [window, test] {
            QObject *panel = window->findChild<QObject *>(u"frameMeter"_s);
            if (!panel) {
                QCoreApplication::exit(1);
                return;
            }
            QObject::connect(panel, SIGNAL(testFinished()), qApp, SLOT(quit()));
            QMetaObject::invokeMethod(panel, "runTest", Q_ARG(QVariant, test));
        });
    }
    if (args.isSet(u"screenshot"_s)) {
        const QString path = args.value(u"screenshot"_s);
        const int delay = args.value(u"delay"_s).toInt();
        QTimer::singleShot(delay, window, [window, path] {
            if (!window->grabWindow().save(path))
                qWarning("legato: could not save %s", qPrintable(path));
            QCoreApplication::quit();
        });
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"legato"_s);
    QGuiApplication::setApplicationDisplayName(u"Legato"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(LEGATO_VERSION));
    // The Wayland app id and the name of the .desktop file.
    QGuiApplication::setDesktopFileName(u"io.github.eeegoloauq.legato"_s);
    // The QML imports QtQuick.Controls.Basic by name. Controls that Qt creates itself, such as the
    // attached ToolTip, go through the generic QtQuick.Controls import, which would otherwise pick
    // the platform's style.
    QQuickStyle::setStyle(u"Basic"_s);
    // libmpv requires the C numeric locale; Qt sets the user's.
    std::setlocale(LC_NUMERIC, "C");

    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    args.addOptions({
        {u"wheel-debug"_s, u"Log mouse wheel deltas and scroll targets for diagnosis."_s},
        {u"appearance"_s, u"Development: system, light or dark, without saving it."_s, u"mode"_s},
        {u"screenshot"_s, u"Development: save the window to <file> and quit."_s, u"file"_s},
        {u"delay"_s, u"Development: milliseconds before --screenshot."_s, u"ms"_s, u"4000"_s},
        {u"scroll"_s, u"Development: scroll the album grid to <px>."_s, u"px"_s},
        {u"open"_s, u"Development: start on <page>, such as albums, or <page>:<id>, such as album:42."_s, u"page"_s},
        {u"full-screen"_s, u"Development: open the full-screen player."_s},
        {u"panel"_s, u"Development: open the side panel on <page>, queue or lyrics."_s, u"page"_s},
        {u"frame-meter"_s, u"Show the frame meter (F2 toggles it)."_s},
        {u"run-test"_s, u"Run the frame meter's scroll or full-screen test, print the result and quit."_s, u"test"_s},
        {u"play"_s, u"Development: play the album in <row> of the albums by name, to silent output "
                    "if there is no sound device."_s, u"row"_s},
    });
    args.process(app);

    QQmlApplicationEngine engine;
    auto *library = engine.singletonInstance<legato::Library *>("Legato", "Library");
    auto *player = engine.singletonInstance<legato::Player *>("Legato", "Player");
    library->setPlayer(player);
    auto *session = engine.singletonInstance<legato::Session *>("Legato", "Session");
    session->setLibrary(library, player);
    if (args.isSet(u"play"_s)) {
        player->setSilentFallback(true);
        const int row = args.value(u"play"_s).toInt();
        QObject::connect(library, &legato::Library::serverChanged, library, [library, row] {
            if (!library->configured())
                return;
            library->client()->albumList(u"alphabeticalByName"_s, 1, row,
                                         [library](const QList<legato::Album> &albums, const QString &) {
                if (!albums.isEmpty())
                    library->playAlbum(albums.constFirst().id);
            });
        }, Qt::SingleShotConnection);
    }
    player->setCovers(library->covers());
    // The engine owns the provider.
    engine.addImageProvider(u"cover"_s, new legato::CoverProvider(library->covers()));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Legato", "Main");
    applyDevOptions(args, engine);

#ifdef Q_OS_LINUX
    // Destroyed before the engine, which owns the player.
    auto mpris = std::make_unique<legato::Mpris>(player, library->covers());
    // The desktop's player controls ask to see the window.
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
        QObject::connect(mpris.get(), &legato::Mpris::raiseRequested, window, [window] {
            window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
            window->show();
            window->raise();
            window->requestActivate();
        });
    }
#endif

    const int code = app.exec();
    // Cover loads still running on the pool use the library's cover store.
    QThreadPool::globalInstance()->waitForDone();
    return code;
}
