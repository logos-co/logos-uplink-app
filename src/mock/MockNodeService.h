#pragma once

#include "interfaces/NodeService.h"

// A node that is Online and core, unless told to report a problem.
class MockNodeService : public node::NodeService {
public:
    explicit MockNodeService(node::Issue issue = node::Issue::None);

    // module | not_running | bootstrapping | not_core | no_peers; anything else is None.
    static node::Issue issueFromName(const QString& name);

    node::Status status() override;
    Result<QByteArray> signWithoutPrompt(const QString& domain, const QByteArray& payload) override;

    void setIssue(node::Issue issue) { m_issue = issue; }

private:
    node::Issue m_issue = node::Issue::None;
};
