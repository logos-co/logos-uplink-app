#!/bin/sh
# Registers an uplink devnet node under an inviter, from the invitation file the uplink CLI reads
# (scripts/invite-to-cli.py makes one from an Uplink invitation code). Run it in the uplink
# checkout (logos-blockchain/uplink) with the devnet up; it does what `just devnet register` does,
# but with an invitation from anyone, Uplink users included:
#
#   sh <this script> 2 devnet/state/invitations/uplink-me.json
set -eu

n=${1:?usage: $0 <node index> <invitation.json>}
invitation=${2:?usage: $0 <node index> <invitation.json>}
state="$PWD/devnet/state"
[ -s "$state/program" ] || { echo "Run this in the uplink checkout, with the devnet up." >&2; exit 1; }
[ -s "$invitation" ] || { echo "$invitation is missing or empty." >&2; exit 1; }

referrer=$(python3 -c 'import json, sys; print(bytes(json.load(open(sys.argv[1]))["parent_node"]).hex())' "$invitation")
node=$(tr -d ' \n' < "$state/config/node-$n/provider_id")

uplink() {   # the uplink CLI, acting as node n's operator
    RISC0_DEV_MODE=1 UPLINK_INDEXER=http://127.0.0.1:8779 UPLINK_PROGRAM="$(cat "$state/program")" \
        LEE_WALLET_HOME_DIR="$state/wallets/node-$n" cargo run -q -p uplink -- "$@"
}

echo "Registering node $n ($node) under node $referrer"
me=$(just devnet participant "$n" | tail -n1)
echo "node $n's participant: $me"
uplink accept-invite --participant "$me" "$invitation"
msg=$(uplink prepare-registration --participant "$me" --node "$node" --referrer "$referrer" | tail -n1)
sig=$(RISC0_DEV_MODE=1 cargo run -q -p uplink_devnet -- sign --key-file "$state/config/node-$n/node.key" "$msg")
uplink attach-signature --participant "$me" --signature "$sig"
uplink register --participant "$me"
just devnet show "$n"
