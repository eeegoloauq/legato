#include "session.h"

#include "library.h"
#include "player.h"

#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

#include <qt6keychain/keychain.h>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#endif

using namespace Qt::Literals::StringLiterals;

namespace legato {

namespace {

// The keychain's service name; each entry's key is the server's id.
constexpr auto KeychainService = "legato"_L1;
constexpr auto OverrideId = "environment"_L1;
// While the server does not answer, legato asks again this often, and when the window comes back.
constexpr int RetryMs = 15'000;

QString settingsPath()
{
    // The file the QML settings use as well.
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.conf"_L1;
}

QString refusedText(const PingResult &result)
{
    if (result.code == 41)
        return Session::tr("This server does not let this account sign in from apps.");
    return Session::tr("Wrong username or password.");
}

} // namespace

bool keychainMissing(int error, bool servicePresent)
{
    switch (error) {
    case QKeychain::NoError:
    case QKeychain::EntryNotFound:
    case QKeychain::AccessDenied:
    case QKeychain::AccessDeniedByUser:
        return false;
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        return true;
    default:
        // OtherError covers both a missing service (D-Bus ServiceUnknown) and a service that
        // refused the entry (oo7-daemon: "Password too short"). Only the bus tells them apart.
        return !servicePresent;
    }
}

bool Session::keychainServicePresent()
{
#ifdef Q_OS_LINUX
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return false;
    const QString name = u"org.freedesktop.secrets"_s;
    if (bus->isServiceRegistered(name))
        return true;
    const QDBusReply<QStringList> activatable = bus->activatableServiceNames();
    return activatable.isValid() && activatable.value().contains(name);
#else
    return true;
#endif
}

Session::Session(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_retry.setInterval(RetryMs);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &Session::retry);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && m_state == Unreachable && !m_busy)
            retry();
    });
}

void Session::setLibrary(Library *library, Player *player)
{
    m_library = library;
    m_player = player;
}

QVariantList Session::servers() const
{
    QVariantList list;
    for (const Server &server : m_list.servers) {
        const bool active = server.id == m_list.activeId;
        Status status = m_status.value(server.id, Status::Unknown);
        if (active)
            status = m_state == Ready ? Status::Up : m_state == Unreachable ? Status::Down : Status::Unknown;
        list.append(QVariantMap{
            {u"id"_s, server.id},
            {u"name"_s, server.name},
            {u"address"_s, server.address},
            {u"username"_s, server.username},
            {u"active"_s, active},
            {u"status"_s, status == Status::Up ? u"up"_s : status == Status::Down ? u"down"_s : u"unknown"_s},
            {u"signedIn"_s, m_passwords.contains(server.id)},
            {u"keychainError"_s, m_keychainErrors.value(server.id)},
        });
    }
    return list;
}

QString Session::activeName() const
{
    const Server *server = m_list.active();
    return server ? server->name : QString();
}

QString Session::activeHost() const
{
    const Server *server = m_list.active();
    return server ? ServerList::defaultName(server->address) : QString();
}

QString Session::fallbackId() const
{
    if (m_state != Unreachable)
        return {};
    for (const Server &server : m_list.servers) {
        if (server.id != m_list.activeId && m_status.value(server.id) == Status::Up)
            return server.id;
    }
    return {};
}

QString Session::fallbackName() const
{
    const Server *server = m_list.find(fallbackId());
    return server ? server->name : QString();
}

void Session::start()
{
    const Credentials environment = Credentials::fromEnvironment();
    if (environment.isComplete()) {
        m_override = true;
        const QString address = subsonic::normalizeAddress(environment.baseUrl);
        m_list.put({OverrideId, ServerList::defaultName(address), address, environment.username});
        m_list.activeId = OverrideId;
        m_passwords.insert(OverrideId, environment.password);
    } else {
        QSettings settings(settingsPath(), QSettings::IniFormat);
        m_list = ServerList::read(settings);
    }
    emit serversChanged();
    if (!m_list.active()) {
        // First run: find out now whether "Keep me signed in" can work.
        withPassword(u"availability-check"_s, [](const QString &, const QString &) {});
        showSignIn({}, nullptr);
        return;
    }
    connectActive();
}

void Session::connectActive()
{
    const Server *active = m_list.active();
    if (!active) {
        showSignIn({}, nullptr);
        return;
    }
    const Server server = *active;
    const int attempt = ++m_attempt;
    setBusy(true);
    withPassword(server.id, [this, server, attempt](const QString &password, const QString &error) {
        if (attempt != m_attempt)
            return;
        if (password.isEmpty()) {
            setBusy(false);
            showSignIn(error, &server);
            return;
        }
        ping({server.address, server.username, password}, [this, server, password, attempt](const PingResult &result) {
            if (attempt != m_attempt)
                return;
            setBusy(false);
            switch (result.kind) {
            case PingResult::Ok:
                becomeReady(server, password);
                break;
            case PingResult::Refused:
                showSignIn(refusedText(result), &server);
                break;
            case PingResult::NotSubsonic:
            case PingResult::Unreachable:
                // A server behind a proxy answers 502 while it is down: for a server that worked
                // before, both mean "not now", not "wrong address".
                qWarning("legato: %s does not answer: %s", qPrintable(server.address), qPrintable(result.detail));
                becomeUnreachable();
                break;
            }
        });
    });
}

void Session::becomeReady(const Server &server, const QString &password)
{
    m_retry.stop();
    m_cancelTo = Starting;
    m_library->setServer({server.address, server.username, password});
    setState(Ready);
    emit serversChanged();
    probeOthers();
}

void Session::becomeUnreachable()
{
    m_cancelTo = Starting;
    setState(Unreachable);
    m_retry.start();
    emit serversChanged();
    probeOthers();
}

void Session::showSignIn(const QString &error, const Server *prefill)
{
    m_error = error;
    m_address = prefill ? prefill->address : QString();
    m_username = prefill ? prefill->username : QString();
    m_retry.stop();
    setState(SignIn);
    emit stateChanged();
}

void Session::probeOthers()
{
    for (const Server &server : std::as_const(m_list.servers)) {
        if (server.id == m_list.activeId)
            continue;
        withPassword(server.id, [this, server](const QString &password, const QString &) {
            if (password.isEmpty()) {
                setStatus(server.id, Status::Unknown);
                return;
            }
            ping({server.address, server.username, password}, [this, id = server.id](const PingResult &result) {
                setStatus(id, result.kind == PingResult::Ok ? Status::Up : Status::Down);
            });
        });
    }
}

void Session::signIn(const QString &address, const QString &username, const QString &password, bool keep)
{
    const QString root = subsonic::normalizeAddress(address);
    const QString user = username.trimmed();
    if (root.isEmpty()) {
        m_error = tr("Enter an address like music.example.org.");
        emit stateChanged();
        return;
    }
    if (user.isEmpty() || password.isEmpty()) {
        m_error = tr("Enter your username and password.");
        emit stateChanged();
        return;
    }
    m_address = root;
    m_username = user;
    const int attempt = ++m_attempt;
    setBusy(true);
    ping({root, user, password}, [this, root, user, password, keep, attempt](const PingResult &result) {
        if (attempt != m_attempt)
            return;
        setBusy(false);
        if (result.kind != PingResult::Ok) {
            m_error = result.kind == PingResult::Refused ? refusedText(result)
                    : result.kind == PingResult::NotSubsonic ? tr("No server at this address. Check it, including the port.")
                                                             : tr("Can't reach this address. Check it, including the port.");
            emit stateChanged();
            return;
        }
        const Server *existing = m_list.match(root, user);
        const Server server = existing ? *existing
                                       : Server{QUuid::createUuid().toString(QUuid::WithoutBraces),
                                                ServerList::defaultName(root), root, user};
        if (server.id != m_list.activeId)
            leaveActive();
        m_list.put(server);
        m_list.activeId = server.id;
        save();
        if (keep && m_keychainAvailable) {
            storePassword(server.id, password);
        } else {
            // A password kept from an earlier sign-in goes, this one stays in memory only.
            forgetPassword(server.id);
        }
        m_passwords.insert(server.id, password);
        becomeReady(server, password);
    });
}

void Session::addServer()
{
    if (m_state == SignIn)
        return;
    const State back = m_state;
    showSignIn({}, nullptr);
    m_cancelTo = back;
    emit stateChanged();
}

void Session::cancelSignIn()
{
    if (m_cancelTo == Starting)
        return;
    ++m_attempt;
    setBusy(false);
    const State back = m_cancelTo;
    m_cancelTo = Starting;
    setState(back);
    if (back == Unreachable)
        m_retry.start();
}

void Session::switchTo(const QString &id)
{
    if (!m_list.find(id) || (id == m_list.activeId && m_state == Ready))
        return;
    leaveActive();
    m_list.activeId = id;
    save();
    m_cancelTo = Starting;
    setState(Starting);
    emit serversChanged();
    connectActive();
}

void Session::retry()
{
    if (m_state == Unreachable)
        connectActive();
}

void Session::signOut(const QString &id)
{
    const Server *server = m_list.find(id);
    if (!server)
        return;
    const Server copy = *server;
    forgetPassword(id);
    if (id == m_list.activeId) {
        leaveActive();
        m_cancelTo = Starting;
        showSignIn({}, &copy);
    }
    emit serversChanged();
}

void Session::remove(const QString &id)
{
    if (!m_list.find(id))
        return;
    forgetPassword(id);
    const bool wasActive = id == m_list.activeId;
    if (wasActive)
        leaveActive();
    m_list.remove(id);
    m_status.remove(id);
    if (wasActive && !m_list.servers.isEmpty())
        m_list.activeId = m_list.servers.first().id;
    save();
    emit serversChanged();
    if (wasActive) {
        m_cancelTo = Starting;
        connectActive();
    }
}

void Session::rename(const QString &id, const QString &name)
{
    if (!m_list.rename(id, name))
        return;
    save();
    emit serversChanged();
}

void Session::leaveActive()
{
    ++m_attempt;
    m_retry.stop();
    // The server first: stopping empties the queue, which must not be saved over the server's.
    if (m_library)
        m_library->setServer({});
    if (m_player)
        m_player->stop();
}

void Session::save()
{
    // With the LEGATO_TEST_* server, the saved list is left as it was.
    if (m_override)
        return;
    QSettings settings(settingsPath(), QSettings::IniFormat);
    m_list.write(settings);
}

void Session::withPassword(const QString &id, std::function<void(QString, QString)> done)
{
    if (const auto known = m_passwords.constFind(id); known != m_passwords.cend()) {
        done(*known, {});
        return;
    }
    if (!m_keychainAvailable || id == OverrideId) {
        done({}, {});
        return;
    }
    auto *job = new QKeychain::ReadPasswordJob(KeychainService, this);
    job->setKey(id);
    connect(job, &QKeychain::Job::finished, this, [this, job, id, done = std::move(done)] {
        switch (job->error()) {
        case QKeychain::NoError:
            m_passwords.insert(id, job->textData());
            done(job->textData(), {});
            break;
        case QKeychain::EntryNotFound:
            done({}, {});
            break;
        default:
            qWarning("legato: keychain read failed (%d): %s", int(job->error()), qPrintable(job->errorString()));
            if (keychainMissing(job->error(), keychainServicePresent())) {
                setKeychainAvailable(false);
                done({}, {});
            } else {
                done({}, tr("The keychain did not give Legato the password (%1). Enter it again.")
                             .arg(job->errorString()));
            }
            break;
        }
    });
    job->start();
}

void Session::storePassword(const QString &id, const QString &password)
{
    auto *job = new QKeychain::WritePasswordJob(KeychainService, this);
    job->setKey(id);
    job->setTextData(password);
    connect(job, &QKeychain::Job::finished, this, [this, job, id] {
        if (job->error() == QKeychain::NoError) {
            if (m_keychainErrors.remove(id))
                emit serversChanged();
            return;
        }
        qWarning("legato: keychain write failed (%d): %s", int(job->error()), qPrintable(job->errorString()));
        if (keychainMissing(job->error(), keychainServicePresent())) {
            setKeychainAvailable(false);
            return;
        }
        // The keychain is there and refused this entry: keep offering it, say why this one failed.
        // The password stays in memory, so this session works; the next start asks again.
        m_keychainErrors.insert(id, tr("The keychain did not save the password: %1").arg(job->errorString()));
        emit serversChanged();
    });
    job->start();
}

void Session::forgetPassword(const QString &id)
{
    m_passwords.remove(id);
    m_keychainErrors.remove(id);
    if (!m_keychainAvailable || id == OverrideId)
        return;
    auto *job = new QKeychain::DeletePasswordJob(KeychainService, this);
    job->setKey(id);
    job->start();
}

void Session::ping(const Credentials &credentials, std::function<void(PingResult)> done)
{
    // A client of its own: the library's keeps talking to the active server meanwhile.
    auto *client = new SubsonicClient(credentials, m_network, this);
    client->ping([client, done = std::move(done)](const PingResult &result) {
        client->deleteLater();
        done(result);
    });
}

void Session::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    emit serversChanged();
}

void Session::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Session::setKeychainAvailable(bool available)
{
    if (m_keychainAvailable == available)
        return;
    m_keychainAvailable = available;
    emit keychainAvailableChanged();
}

void Session::setStatus(const QString &id, Status status)
{
    if (m_status.value(id, Status::Unknown) == status && m_status.contains(id))
        return;
    m_status.insert(id, status);
    emit serversChanged();
}

} // namespace legato
