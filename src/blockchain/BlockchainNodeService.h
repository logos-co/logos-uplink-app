#pragma once

#include "interfaces/NodeService.h"

class LogosUiPluginContext;

// NodeService over blockchain_module. The provider_id waits on
// logos-blockchain-module#108 (blend_status).
class BlockchainNodeService : public node::NodeService {
public:
    explicit BlockchainNodeService(LogosUiPluginContext& context);

    node::Status status() override;

private:
    LogosUiPluginContext& m_context;
};
