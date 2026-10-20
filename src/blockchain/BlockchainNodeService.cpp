#include "blockchain/BlockchainNodeService.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_sdk.h"
#include "logos_ui_plugin_context.h"
#include "NodeRules.h"

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
    if (!node::applyMode(s, jsonObject(info.value).value(QStringLiteral("mode")).toString()))
        return s;

    const LogosResult chain = logos.blockchain_module.get_chain_id();
    if (chain.success)
        s.chainId = chain.value.toString();

    const LogosResult blend = logos.blockchain_module.blend_info();
    if (!blend.success) {
        s.issue = Issue::NotRunning;
        s.detail = blend.error.toString();
        return s;
    }
    // The node's id, from the config it was started with (read_accounts with no path).
    QString nodeId;
    const LogosResult accounts = logos.blockchain_module.read_accounts(QString());
    if (accounts.success)
        nodeId = node::blendSigningKey(jsonObject(accounts.value));
    node::applyBlend(s, jsonObject(blend.value), nodeId);
    return s;
}

