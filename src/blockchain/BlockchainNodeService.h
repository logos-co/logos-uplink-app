#pragma once

#include "interfaces/NodeService.h"

class LogosUiPluginContext;

// NodeService over blockchain_module. The node's id (provider_id) comes from read_accounts with
// no path, which reads the config the running node was started with.
class BlockchainNodeService : public node::NodeService {
public:
    explicit BlockchainNodeService(LogosUiPluginContext& context);

    node::Status status() override;

private:
    LogosUiPluginContext& m_context;
};
