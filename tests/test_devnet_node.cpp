// The uplink devnet's node API answers, recorded from a running devnet (logos-blockchain
// 3926a53), read by devnet::status with the same rules as blockchain_module's.

#include <logos_test.h>

#include "devnet/DevnetNodeJson.h"

using node::Issue;

namespace {

const QByteArray kOnline = R"({"cryptarchia_info":{"lib":"05f1","lib_slot":3767,"tip":"298a","slot":3771,"height":1827,"state":"Online"},"phase":"Following"})";
const QByteArray kChain = R"({"chain_id":"process_start_nonce=18dfe08a43c0df45-00000001, test_entropy=e856611e030cf414"})";
const QByteArray kNotCore = R"({"node_id":"12D3KooWDdLJEYscypJnuMLRPofLfvq6NcUC1vbc3rmpgqbweXn2","core_info":null})";
const QByteArray kCore = R"({"node_id":"12D3KooW","core_info":{"current_epoch_peers":[["12D3a",true],["12D3b",false]]}})";
const QByteArray kCoreNoPeers = R"({"node_id":"12D3KooW","core_info":{"current_epoch_peers":[["12D3a",false]]}})";

} // namespace

LOGOS_TEST(a_devnet_node_online_but_not_blend_core_says_so) {
    const auto s = devnet::status(kOnline, kChain, kNotCore, {});
    LOGOS_ASSERT_TRUE(s.issue == Issue::NotCore);
    LOGOS_ASSERT_EQ(s.mode, QString("Online"));
    LOGOS_ASSERT_TRUE(s.chainId.startsWith("process_start_nonce="));
}

LOGOS_TEST(a_blend_core_devnet_node_counts_healthy_peers) {
    const auto s = devnet::status(kOnline, kChain, kCore, {});
    LOGOS_ASSERT_TRUE(s.core);
    LOGOS_ASSERT_EQ(s.healthyBlendPeers, 1);
    LOGOS_ASSERT_TRUE(s.issue == Issue::NodeIdUnavailable);   // as with blockchain_module until #108
    LOGOS_ASSERT_TRUE(devnet::status(kOnline, kChain, kCoreNoPeers, {}).issue == Issue::NoBlendPeers);
}

LOGOS_TEST(a_given_provider_id_names_the_devnet_node) {
    const auto s = devnet::status(kOnline, kChain, kCore, QStringLiteral("389b3e50"));
    LOGOS_ASSERT_TRUE(s.issue == Issue::None);
    LOGOS_ASSERT_EQ(s.nodeId, QString("389b3e50"));
}

LOGOS_TEST(a_bootstrapping_or_silent_devnet_node_is_not_running_yet) {
    const QByteArray bootstrapping = R"({"cryptarchia_info":{"state":"Bootstrapping"},"phase":"Following"})";
    LOGOS_ASSERT_TRUE(devnet::status(bootstrapping, kChain, kCore, {}).issue == Issue::Bootstrapping);
    LOGOS_ASSERT_TRUE(devnet::status({}, {}, {}, {}).issue == Issue::NotRunning);
    const auto silent = devnet::unreachable(QStringLiteral("No answer"));
    LOGOS_ASSERT_TRUE(silent.issue == Issue::NotRunning);
    LOGOS_ASSERT_EQ(silent.detail, QString("No answer"));
}
