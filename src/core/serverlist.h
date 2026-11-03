// The saved servers: address, username and name, and which one is active. Passwords are never
// here; they are in the keychain under the server's id.
#pragma once

#include <QList>
#include <QString>

class QSettings;

namespace legato {

struct Server {
    // A random id, stable across renames and address changes; the keychain entry's key.
    QString id;
    QString name;
    // Normalized (subsonic::normalizeAddress).
    QString address;
    QString username;
};

struct ServerList {
    QList<Server> servers;
    QString activeId;

    static ServerList read(QSettings &settings);
    void write(QSettings &settings) const;

    const Server *find(const QString &id) const;
    const Server *active() const { return find(activeId); }
    // The saved server with this address and username, so signing in again does not add a copy.
    const Server *match(const QString &address, const QString &username) const;
    // Adds or replaces by id.
    void put(const Server &server);
    void remove(const QString &id);
    // A name of the person's choosing; an empty one goes back to the host name. Returns whether the
    // name changed.
    bool rename(const QString &id, const QString &name);

    // The host name, which is what people call their server until they rename it.
    static QString defaultName(const QString &address);
};

} // namespace legato
