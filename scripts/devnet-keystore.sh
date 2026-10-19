#!/bin/sh
# Writes a keystore.yaml for one uplink devnet node (logos-blockchain/uplink, `just devnet up`),
# in the format uplink_module reads, so Uplink can sign as that node:
#
#   scripts/devnet-keystore.sh <uplink>/devnet/state/config/node-1
#
# The devnet keeps each node's Ed25519 key as node.key (its BlendSigning seed) and the public
# half as provider_id. The file is written next to them, readable only by you.
set -eu

dir=${1:?usage: $0 <devnet state>/config/node-N}
key="$dir/node.key"
provider="$dir/provider_id"
[ -r "$key" ] && [ -r "$provider" ] || { echo "$dir has no node.key and provider_id" >&2; exit 1; }

out="$dir/keystore.yaml"
umask 077
{
    echo "public_keys:"
    echo "  BlendSigning: $(tr -d ' \n' < "$provider")"
    echo "secret_keys:"
    echo "  BlendSigning: !Ed25519 $(tr -d ' \n' < "$key")"
} > "$out"
echo "$out"
echo "Node ID (UPLINK_NODE_ID): $(tr -d ' \n' < "$provider")"
