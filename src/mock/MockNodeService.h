#pragma once

#include <QString>
#include <QVariantMap>

#include "interfaces/NodeService.h"

// A node that is Online and core, unless told to report a problem.
class MockNodeService : public node::NodeService {
public:
    explicit MockNodeService(node::Issue issue = node::Issue::None);

    // module | not_running | bootstrapping | not_core | no_peers; anything else is None.
    static node::Issue issueFromName(const QString& name);

    node::Status status() override;
    // The node's id (its BlendSigning public key).
    static QString nodeId();

    // What the node app answers a referral request ({intent, params}) with, once the user
    // approves: {signature, public_key} or {receipt_secret, public_key} (MockSigning.h).
    static QVariantMap answerRequest(const QVariantMap& request);

    void setIssue(node::Issue issue) { m_issue = issue; }
    // Another node than the one nodeId() names: as when the wallet's account joined with that
    // one (mock:other_node).
    void setRunningNodeId(const QString& id) { m_nodeId = id; }

private:
    node::Issue m_issue = node::Issue::None;
    QString m_nodeId = nodeId();
};
