#include "blockchain/BlockchainNodeService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_sdk.h"
#include "logos_ui_plugin_context.h"

using node::Issue;

namespace {

const char kModule[] = "blockchain_module";

QJsonObject jsonObject(const QVariant& value)
{
    return QJsonDocument::fromJson(value.toString().toUtf8()).object();
}

} // namespace

BlockchainNodeService::BlockchainNodeService(LogosUiPluginContext& context)
    : m_context(context)
{
}

node::Status BlockchainNodeService::status()
{
    node::Status s;
    if (!m_context.isContextReady())
        return s;
    LogosModules& logos = m_context.modules();
    LogosAPIClient* client = logos.api->getClient(kModule);
    if (!client || !client->isConnected())
        return s;

    const LogosResult info = logos.blockchain_module.get_cryptarchia_info();
    if (!info.success) {
        s.issue = Issue::NotRunning;
        s.detail = info.error.toString();
        return s;
    }
    s.mode = jsonObject(info.value).value(QStringLiteral("mode")).toString();
    if (s.mode != QLatin1String("Online")) {
        s.issue = s.mode == QLatin1String("Bootstrapping") ? Issue::Bootstrapping : Issue::NotRunning;
        return s;
    }

    const LogosResult chain = logos.blockchain_module.get_chain_id();
    if (chain.success)
        s.chainId = chain.value.toString();

    // {"node_id": PeerId, "core_info": null | {"current_epoch_peers": [[PeerId, healthy], ...], ...}}
    const LogosResult blend = logos.blockchain_module.blend_info();
    if (!blend.success) {
        s.issue = Issue::NotRunning;
        s.detail = blend.error.toString();
        return s;
    }
    const QJsonValue coreInfo = jsonObject(blend.value).value(QStringLiteral("core_info"));
    s.core = coreInfo.isObject();
    if (!s.core) {
        s.issue = Issue::NotCore;
        return s;
    }
    for (const QJsonValue peer : coreInfo.toObject().value(QStringLiteral("current_epoch_peers")).toArray())
        if (peer.toArray().at(1).toBool())
            ++s.healthyBlendPeers;
    if (s.healthyBlendPeers == 0) {
        s.issue = Issue::NoBlendPeers;
        return s;
    }

    s.issue = Issue::NodeIdUnavailable;
    return s;
}

Result<QByteArray> BlockchainNodeService::signWithoutPrompt(const QString&, const QByteArray&)
{
    return Result<QByteArray>::failure(QStringLiteral("the node app signs, on the user's approval"));
}
