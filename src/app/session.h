// Which server legato talks to, and whether it can (docs/design.md, "Sign-in and servers").
//
// At start it pings the active server and decides: the sign-in card when there is no server or the
// server refuses the credentials, the app when it answers, "Can't reach" when it does not. Passwords
// live in the system keychain (QtKeychain) when "Keep me signed in" is on, and in memory otherwise.
#pragma once

#include "serverlist.h"
#include "subsonic.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQmlIntegration>

#include <functional>

class QNetworkAccessManager;

namespace legato {

class Library;
class Player;

// Whether a failed keychain job means there is no keychain at all, rather than that the keychain
// refused this one entry. `error` is a QKeychain::Error; `servicePresent` says whether a keychain
// service is running or startable (Session::keychainServicePresent).
bool keychainMissing(int error, bool servicePresent);

class Session : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // The sign-in card: the line above the fields and what the fields start with.
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString address READ address NOTIFY stateChanged)
    Q_PROPERTY(QString username READ username NOTIFY stateChanged)
    // True when the card was opened over a working app (Add Server), so it can be closed again.
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
    // A sign-in or a retry is under way.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool keychainAvailable READ keychainAvailable NOTIFY keychainAvailableChanged)
    // { id, name, address, username, active, status: "up" | "down" | "unknown", signedIn,
    //   keychainError: why the keychain refused this server's password, empty when it did not }
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QString activeName READ activeName NOTIFY serversChanged)
    Q_PROPERTY(QString activeHost READ activeHost NOTIFY serversChanged)
    // Another saved server that answers while the active one does not; empty if none.
    Q_PROPERTY(QString fallbackId READ fallbackId NOTIFY serversChanged)
    Q_PROPERTY(QString fallbackName READ fallbackName NOTIFY serversChanged)

public:
    enum State { Starting, SignIn, Ready, Unreachable };
    Q_ENUM(State)

    // On Linux: a Secret Service (gnome-keyring, KWallet, oo7-daemon, KeePassXC) is registered on the
    // session bus or can be started by it. Elsewhere the system keychain is always there.
    static bool keychainServicePresent();

    explicit Session(QObject *parent = nullptr);

    void setLibrary(Library *library, Player *player);

    State state() const { return m_state; }
    QString error() const { return m_error; }
    QString address() const { return m_address; }
    QString username() const { return m_username; }
    bool canCancel() const { return m_cancelTo != Starting; }
    bool busy() const { return m_busy; }
    bool keychainAvailable() const { return m_keychainAvailable; }
    QVariantList servers() const;
    QString activeName() const;
    QString activeHost() const;
    QString fallbackId() const;
    QString fallbackName() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void signIn(const QString &address, const QString &username, const QString &password, bool keep);
    Q_INVOKABLE void addServer();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void switchTo(const QString &id);
    Q_INVOKABLE void retry();
    // Forgets the password, here and in the keychain. The server stays in the list.
    Q_INVOKABLE void signOut(const QString &id);
    Q_INVOKABLE void remove(const QString &id);
    // An empty name goes back to the host name.
    Q_INVOKABLE void rename(const QString &id, const QString &name);

signals:
    void stateChanged();
    void busyChanged();
    void keychainAvailableChanged();
    void serversChanged();

private:
    enum class Status { Unknown, Up, Down };

    // The password from memory, else from the keychain. Empty when there is none; `error` says why
    // when the keychain failed rather than had nothing.
    void withPassword(const QString &id, std::function<void(QString password, QString error)> done);
    void storePassword(const QString &id, const QString &password);
    void forgetPassword(const QString &id);
    void ping(const Credentials &credentials, std::function<void(PingResult)> done);

    void connectActive();
    void becomeReady(const Server &server, const QString &password);
    void becomeUnreachable();
    void showSignIn(const QString &error, const Server *prefill);
    void probeOthers();
    void leaveActive();
    void save();
    void setState(State state);
    void setBusy(bool busy);
    void setKeychainAvailable(bool available);
    void setStatus(const QString &id, Status status);

    QNetworkAccessManager *m_network;
    Library *m_library = nullptr;
    Player *m_player = nullptr;
    ServerList m_list;
    QHash<QString, QString> m_passwords;
    QHash<QString, Status> m_status;
    QHash<QString, QString> m_keychainErrors;
    // The LEGATO_TEST_* server: used, never saved, never in the keychain.
    bool m_override = false;
    State m_state = Starting;
    // Where Cancel on the card returns to; Starting when the card cannot be cancelled.
    State m_cancelTo = Starting;
    QString m_error;
    QString m_address;
    QString m_username;
    bool m_busy = false;
    bool m_keychainAvailable = true;
    // Counts connection attempts, so the answer to an older one is dropped.
    int m_attempt = 0;
    QTimer m_retry;
};

} // namespace legato
