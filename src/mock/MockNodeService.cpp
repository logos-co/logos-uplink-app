#include "mock/MockNodeService.h"

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
    if (m_issue == Issue::None)
        s.nodeId = kNodeId;
    return s;
}
