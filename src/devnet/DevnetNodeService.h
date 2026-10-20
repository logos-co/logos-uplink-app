#pragma once

#include <functional>

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include "interfaces/NodeService.h"

// For testing against the uplink devnet (logos-blockchain/uplink, `just devnet up`): one of its
// nodes, read over its HTTP API, stands in for the node in Basecamp. Selected with
// UPLINK_NODE=devnet:<node API URL>; UPLINK_NODE_ID (its provider_id) makes Uplink check the
// node app's answers against it. Polls in the background, so status() never waits on the network.
class DevnetNodeService : public QObject, public node::NodeService {
public:
    DevnetNodeService(const QUrl& api, const QString& nodeId, QObject* parent = nullptr);

    node::Status status() override { return m_status; }

private:
    void poll();
    void get(const QString& path, std::function<void(const QByteArray&)> then);
    void fail(const QString& error);

    QNetworkAccessManager m_net;
    QTimer m_timer;
    QUrl m_api;
    QString m_nodeId;
    node::Status m_status;
    bool m_polling = false;
    QByteArray m_info;
    QByteArray m_chain;
};
