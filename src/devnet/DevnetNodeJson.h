#pragma once

#include <QJsonDocument>
#include <QJsonObject>

#include "NodeRules.h"

// A logos-blockchain node's HTTP API answers (as the uplink devnet's nodes serve them), read into
// a node::Status by the same rules as blockchain_module's. Kept apart from the HTTP so tests can
// check it with recorded answers.
//   GET /cryptarchia/info  {"cryptarchia_info": {..., "state": "Online"}, "phase": ...}
//   GET /chain/id          {"chain_id": "..."}
//   GET /blend/info        {"node_id": PeerId, "core_info": null | {...}}
namespace devnet {

inline QJsonObject object(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

inline node::Status status(const QByteArray& cryptarchiaInfo, const QByteArray& chainId,
                           const QByteArray& blendInfo, const QString& nodeId)
{
    node::Status s;
    const QString mode = object(cryptarchiaInfo).value(QStringLiteral("cryptarchia_info")).toObject()
                             .value(QStringLiteral("state")).toString();
    if (!node::applyMode(s, mode))
        return s;
    s.chainId = object(chainId).value(QStringLiteral("chain_id")).toString();
    node::applyBlend(s, object(blendInfo), nodeId);
    return s;
}

// The node didn't answer; the user's fix is the same as for a stopped node.
inline node::Status unreachable(const QString& detail)
{
    node::Status s;
    s.issue = node::Issue::NotRunning;
    s.detail = detail;
    return s;
}

} // namespace devnet
