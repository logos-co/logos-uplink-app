#pragma once

#include <QByteArray>
#include <QString>

#include "interfaces/Result.h"

// The local node's status, through blockchain_module. Signing with the node key is
// not here: the node app does it, on the user's approval, via the
// node.sign_message intent (see uplink_ui.rep).
namespace node {

// Ordered: the first one that applies is the one the user fixes first.
enum class Issue { None, ModuleUnavailable, NotRunning, Bootstrapping, NotCore, NoBlendPeers, NodeIdUnavailable };

struct Status {
    Issue issue = Issue::ModuleUnavailable;
    QString detail;          // the module's own message, when it gave one
    QString mode;            // Online | Bootstrapping | NotStarted
    bool core = false;
    int healthyBlendPeers = 0;
    QString nodeId;          // BlendSigning provider_id
    QString chainId;
};

class NodeService {
public:
    virtual ~NodeService() = default;

    virtual Status status() = 0;
    virtual Result<QByteArray> signWithoutPrompt(const QString& domain, const QByteArray& payload) = 0;
};

} // namespace node
