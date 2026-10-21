#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "interfaces/NodeService.h"

// How a node's readings become a node::Status, shared by every node source (blockchain_module,
// a devnet node's HTTP API) so they can't disagree. Each step returns false once it has set the
// issue the user should fix first.
namespace node {

// Online goes on; Bootstrapping and anything else stop here.
inline bool applyMode(Status& s, const QString& mode)
{
    s.mode = mode;
    if (mode == QLatin1String("Online"))
        return true;
    s.issue = mode == QLatin1String("Bootstrapping") ? Issue::Bootstrapping : Issue::NotRunning;
    return false;
}

// blend_info: {"node_id": PeerId, "core_info": null | {"current_epoch_peers": [[PeerId, healthy], ...], ...}}.
// The last step: a core node with healthy peers is fine once its id is known. The id is the
// node's whether or not it's core, so it's kept either way.
inline void applyBlend(Status& s, const QJsonObject& blendInfo, const QString& nodeId)
{
    s.nodeId = nodeId;
    const QJsonValue coreInfo = blendInfo.value(QStringLiteral("core_info"));
    s.core = coreInfo.isObject();
    if (!s.core) {
        s.issue = Issue::NotCore;
        return;
    }
    s.healthyBlendPeers = 0;
    for (const QJsonValue peer : coreInfo.toObject().value(QStringLiteral("current_epoch_peers")).toArray())
        if (peer.toArray().at(1).toBool())
            ++s.healthyBlendPeers;
    if (s.healthyBlendPeers == 0) {
        s.issue = Issue::NoBlendPeers;
        return;
    }
    s.issue = nodeId.isEmpty() ? Issue::NodeIdUnavailable : Issue::None;
}

inline QString blendSigningKey(const QJsonObject& accounts)
{
    for (const QJsonValue a : accounts.value(QStringLiteral("accounts")).toArray()) {
        const QJsonObject account = a.toObject();
        if (account.value(QStringLiteral("roles")).toArray().contains(QStringLiteral("blend_signing")))
            return account.value(QStringLiteral("public_key")).toString().toLower();
    }
    return {};
}

} // namespace node
