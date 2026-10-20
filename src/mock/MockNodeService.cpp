#include "mock/MockNodeService.h"
#include "mock/MockSigning.h"

#include <QCryptographicHash>

using node::Issue;

namespace {

const QString kNodeId = QString::fromLatin1(
    QCryptographicHash::hash("mock-node", QCryptographicHash::Sha256).toHex());

} // namespace

Issue MockNodeService::issueFromName(const QString& name)
{
    if (name == QLatin1String("module")) return Issue::ModuleUnavailable;
    if (name == QLatin1String("not_running")) return Issue::NotRunning;
    if (name == QLatin1String("bootstrapping")) return Issue::Bootstrapping;
    if (name == QLatin1String("not_core")) return Issue::NotCore;
    if (name == QLatin1String("no_peers")) return Issue::NoBlendPeers;
    return Issue::None;
}

MockNodeService::MockNodeService(Issue issue)
    : m_issue(issue)
{
}

node::Status MockNodeService::status()
{
    node::Status s;
    s.issue = m_issue;
    switch (m_issue) {
    case Issue::ModuleUnavailable:
        return s;
    case Issue::NotRunning:
        s.mode = QStringLiteral("NotStarted");
        s.detail = QStringLiteral("The node is not running.");
        return s;
    case Issue::Bootstrapping:
        s.mode = QStringLiteral("Bootstrapping");
        return s;
    default:
        break;
    }
    s.mode = QStringLiteral("Online");
    s.chainId = QStringLiteral("mock-testnet");
    s.core = m_issue != Issue::NotCore;
    s.healthyBlendPeers = s.core && m_issue != Issue::NoBlendPeers ? 4 : 0;
    s.nodeId = m_issue == Issue::NodeIdUnavailable ? QString() : kNodeId;   // as with a real node, #108
    return s;
}

QString MockNodeService::nodeId() { return kNodeId; }

QVariantMap MockNodeService::answerRequest(const QVariantMap& request)
{
    const QString intent = request.value(QStringLiteral("intent")).toString();
    if (intent == mock_signing::kReceiptSecretIntent)
        return {{QStringLiteral("receipt_secret"), mock_signing::receiptSecret(kNodeId)},
                {QStringLiteral("public_key"), kNodeId}};
    return {{QStringLiteral("signature"), mock_signing::signature(kNodeId, intent, request.value(QStringLiteral("params")).toMap())},
            {QStringLiteral("public_key"), kNodeId}};
}
