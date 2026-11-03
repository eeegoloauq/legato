#include "serverlist.h"

#include <QSettings>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

namespace legato {

ServerList ServerList::read(QSettings &settings)
{
    ServerList list;
    settings.beginGroup("servers"_L1);
    list.activeId = settings.value("active"_L1).toString();
    const int count = settings.beginReadArray("list"_L1);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Server server{settings.value("id"_L1).toString(), settings.value("name"_L1).toString(),
                      settings.value("address"_L1).toString(), settings.value("username"_L1).toString()};
        if (!server.id.isEmpty() && !server.address.isEmpty())
            list.servers.append(server);
    }
    settings.endArray();
    settings.endGroup();
    return list;
}

void ServerList::write(QSettings &settings) const
{
    settings.beginGroup("servers"_L1);
    settings.setValue("active"_L1, activeId);
    settings.remove("list"_L1);
    settings.beginWriteArray("list"_L1, int(servers.size()));
    for (int i = 0; i < servers.size(); ++i) {
        settings.setArrayIndex(i);
        const Server &server = servers.at(i);
        settings.setValue("id"_L1, server.id);
        settings.setValue("name"_L1, server.name);
        settings.setValue("address"_L1, server.address);
        settings.setValue("username"_L1, server.username);
    }
    settings.endArray();
    settings.endGroup();
}

const Server *ServerList::find(const QString &id) const
{
    for (const Server &server : servers) {
        if (server.id == id)
            return &server;
    }
    return nullptr;
}

const Server *ServerList::match(const QString &address, const QString &username) const
{
    for (const Server &server : servers) {
        if (server.address == address && server.username == username)
            return &server;
    }
    return nullptr;
}

void ServerList::put(const Server &server)
{
    for (Server &existing : servers) {
        if (existing.id == server.id) {
            existing = server;
            return;
        }
    }
    servers.append(server);
}

void ServerList::remove(const QString &id)
{
    servers.removeIf([&id](const Server &server) { return server.id == id; });
    if (activeId == id)
        activeId.clear();
}

bool ServerList::rename(const QString &id, const QString &name)
{
    for (Server &server : servers) {
        if (server.id != id)
            continue;
        // Spaces and line breaks pasted in would only show as gaps.
        const QString wanted = name.simplified();
        const QString next = wanted.isEmpty() ? defaultName(server.address) : wanted;
        if (next == server.name)
            return false;
        server.name = next;
        return true;
    }
    return false;
}

QString ServerList::defaultName(const QString &address)
{
    const QString host = QUrl(address).host();
    return host.isEmpty() ? address : host;
}

} // namespace legato
