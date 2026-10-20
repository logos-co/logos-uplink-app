# Uplink

Basecamp app for the Logos node referral programme: node operators invite other node operators, and
earn points while the nodes they brought in do real work on the network.

> Status: early development. The programme's rules are not final, and the backend it needs is not
> released yet (see [Dependencies](#dependencies)).

## How the programme works

- **Join under someone.** You import the invitation from the person who referred you, then enrol.
  Enrolling binds your node to that referrer permanently. There are no referral codes: the bind is
  signed by your node's Blend signing key. You can also enrol without a referrer and start your own tree.
- **Earn while your referrals are active.** For every epoch a node you referred is active, you
  earn points. A node counts as active when it is a Blend core node that proves its work on-chain
  for that epoch. You earn nothing for your own node.
- **Your tree, not just your referrals.** When someone you referred collects their points, a share
  is credited up the tree to you.
- **Collect, then cash out.** Points are not tokens. *Collect* moves your earned points into your
  balance on the Logos Execution Zone (LEZ). *Cash out* turns the whole balance into a receipt that
  you hand to Logos for payout.
- **Private by default.** Your points and referral links are kept private on LEZ. You see only your
  direct referrals; any labels you give them stay on your machine. Each cash-out publicly links your
  node to an amount, so cash out rarely.

## What you need

- The Logos Basecamp app, with the Blockchain app running a node that is **Online** and declared as
  a **Blend core node**.
- A LEZ wallet. Your points account lives in it and can be restored from its recovery phrase.

## Dependencies

| Piece | Provides | Status |
|---|---|---|
| [Uplink referral program](https://github.com/logos-blockchain/uplink) | Registration, points, claims, cash-out | Merged; needs deploying per network, with its oracle |
| `uplink_module` | The programme for Basecamp: terms, enrolment, claims, cash-out, payout codes. It never sees the node's key: it takes signatures and the receipt secret, and checks them | [logos-blockchain/uplink-module](https://github.com/logos-blockchain/uplink-module) |
| `lez_core` ([logos-execution-zone-module](https://github.com/logos-blockchain/logos-execution-zone-module)) | The wallet: keys, sync, private transactions | Needs [#69](https://github.com/logos-blockchain/logos-execution-zone-module/pull/69) (`send_generic_private_transaction_with_accounts`) |
| `blockchain_module` ([logos-blockchain-module](https://github.com/logos-blockchain/logos-blockchain-module)) | Node status, Blend core role | Status available; the node's `provider_id` waits on [#108](https://github.com/logos-blockchain/logos-blockchain-module/issues/108). Until then the Blockchain app's answers name the node |
| Blockchain app ([logos-blockchain-ui](https://github.com/logos-blockchain/logos-blockchain-ui)) | Holds the node's key: signs the enrolment and payout codes, hands over the receipt secret, each on the user's approval | The `node.sign_referral_*` and `node.referral_receipt_secret` intents; not merged yet |

## Development

A `ui_qml` module: a QML view plus a C++ backend (`UplinkBackend`), with the contract in
`src/uplink_ui.rep`.

### Layout

```
src/uplink_ui.rep           the contract the view binds to
src/UplinkBackend.*         app state: participant, operation refs, labels, points totals
src/interfaces/             interfaces
  ReferralService.h           uplink_module's calls, 1:1, plus lez_core's wallet calls
  NodeService.h               the local node: status, Blend role
src/lez/                    ReferralService over uplink_module and lez_core
  ReferralJson.h              uplink_module's replies, read (tested without the SDK)
src/blockchain/             NodeService over blockchain_module
src/devnet/                 NodeService over an uplink devnet node's HTTP API (testing)
src/mock/                   in-memory implementations of both
src/qml/Main.qml            placeholder view
tests/                      unit, backend and UI tests (see Build and test)
doctests/                   the mock walkthrough, as a doc-test
```

### Real vs mock

Both sides are real by default. Set at launch, read once:

| Variable | Values | Effect |
|---|---|---|
| `UPLINK_BACKEND` | `mock`, `real` | Both sides. When set, the two below are ignored. |
| `UPLINK_LEZ` | `mock`, `mock:no_wallet`, `mock:no_program`, `real` | Referral side. `real` uses uplink_module for the programme and lez_core for the wallet. The deployment comes from `metadata.json` (below). `mock:no_wallet` makes the mock report that no wallet is open; `mock:no_program` that no deployment is selected. |
| `UPLINK_NODE` | `mock`, `mock:<issue>`, `devnet:<url>`, `real` | Node side. `real` uses blockchain_module: status works today (`get_cryptarchia_info`, `get_chain_id`, `blend_info`); the node's `provider_id` waits on logos-blockchain-module#108. `mock:<issue>` makes the mock node report `module`, `not_running`, `bootstrapping`, `not_core` or `no_peers`. `devnet:<url>` reads an uplink devnet node's HTTP API instead; `UPLINK_NODE_ID` gives its provider ID. |

### Which deployment

A release targets one network. `metadata.json` names it:

```json
"referral_program": { "chain_id": "", "program_account": "<program account, base58 or hex>" }
```

`program_account` is the account the referral program was deployed at (`just devnet up` in
logos-blockchain/uplink prints it). `chain_id` is the node's chain ID, as Uplink's network tag
shows it; when set, Uplink refuses to run against a node on another chain. Uplink reads them at
startup from the `metadata.json` installed next to its plugin, as the Blockchain app reads its
`bootstrap_peers`, and selects the deployment in uplink_module on every refresh; empty leaves
the module's own choice (`configure` or `UPLINK_PROGRAM`). The mock ignores both.

```bash
nix run .                              # everything real
UPLINK_BACKEND=mock nix run .          # everything mock
UPLINK_LEZ=mock nix run .              # real node, mock referral programme
UPLINK_NODE=mock nix run .             # mock node, real referral programme
UPLINK_NODE=mock:not_core nix run .    # mock node that is not a core node
```

The mock referral programme follows the program's rules: points accrue (a claim pays every epoch a
referral was active, and your own node was too, since it was last settled), cash-out takes the whole
balance, and a signature or receipt secret that isn't the node's is refused. With the mock node,
a mock node app answers the signing requests at once (`MockNodeService::answerRequest`), so mock
runs need no Blockchain app.

It settles operations on the next poll (5 s), seeds three children when you
register, and publishes a new epoch every 30 s (`UPLINK_MOCK_EPOCH_MS` changes that). The only
registered node at start is `MockReferralService::inviterNode()`.

### Testing on uplink's devnet

[logos-blockchain/uplink](https://github.com/logos-blockchain/uplink)'s devnet runs four nodes, the
LEZ sequencer and indexer, and the oracle in Docker. Uplink joins as one of its nodes (here node 1):
the Blockchain app signs with that node's key, and Uplink reads the node's status from its HTTP API.
The Blockchain app doesn't run the node; its node (0.3.0-rc.5) can't read the devnet's configs or
sync with its nodes.

1. **Start the devnet**, in the uplink checkout, with node 1 in Blend and funded. `fund-sdp.sh` is in
   uplink_module's `devnet/`: genesis funds the nodes' Blend activity for only a few epochs.

   ```bash
   just devnet up
   sh <uplink_module>/devnet/fund-sdp.sh
   just devnet blend-join 1      # a member two epochs later
   ```

2. **Give node 1 a keystore** the Blockchain app can sign from, with uplink_module's `devnet/keystore.sh`
   (in the uplink checkout). `just devnet reset` deletes it with the rest of `devnet/state`, so run
   it again after each reset.

   ```bash
   sh <uplink_module>/devnet/keystore.sh 1
   ```

3. **Run Uplink**, from this repo. `RISC0_DEV_MODE=1` skips proving, as the devnet's own services do:
   a real proof takes minutes and can take lez_core down. Use a new `--user-dir` after every reset,
   since the old one remembers the previous chain.

   ```bash
   RISC0_DEV_MODE=1 \
   UPLINK_PROGRAM=$(cat <uplink>/devnet/state/program) \
   UPLINK_NODE=devnet:http://127.0.0.1:18081 UPLINK_NODE_ID=$(cat <uplink>/devnet/state/config/node-1/provider_id) \
   nix run . -- \
     --app blockchain_ui=path:../logos-blockchain-ui \
     --app lez_wallet_ui=path:../logos-execution-zone-wallet-ui \
     --user-dir /tmp/uplink-devnet
   ```

4. **In the Blockchain app**, choose `<uplink>/devnet/state/config/node-1/config.yaml`. When it says
   the config is out of date, press **Not now**: don't update it, start fresh or start the node.

5. **In the LEZ Wallet**, choose **Create a new wallet**, then **Localhost** for the network
   (`http://127.0.0.1:3040`, the devnet's sequencer), set a password and keep the recovery phrase.
   Uplink creates your points account in it when you join.

6. **For claim history, start the LEZ indexer** in the LEZ Explorer app (Uplink's banner opens it),
   pointed at the devnet: Bedrock address `http://127.0.0.1:18080` and channel
   `0101010101010101010101010101010101010101010101010101010101010101`. Its settings are kept
   outside `--user-dir`, so one left on another chain carries over; reset its storage after
   changing them. Without it, epochs from before Uplink was running can't be claimed, and the
   overview says so.

7. **Join in Uplink.** The Blockchain app asks you to approve, showing node 1 as **Your node**; then
   Uplink shows *Joining…* until the registration settles.

**After `just devnet reset`**, the chain, the program and every account are new, so:

- redo steps 1 and 2 (the keystore went with `devnet/state`);
- run Uplink with a new `--user-dir`;
- make the LEZ Wallet forget its wallet. It remembers where the wallet is in its own settings, not
  under `--user-dir`, so a new `--user-dir` alone reopens the old wallet, whose accounts were on the
  old chain. On macOS, with Basecamp closed: `defaults delete com.Logos.ExecutionZoneWalletUI`
  (`defaults domains | tr , '\n' | grep -i logos` lists the name if it differs). Then do step 5 again.

The Blockchain app needs nothing: the config path is the same; restart it so it reads the new keystore.

`just devnet dashboard` shows the result: node 1 under **Uplink registry** once it's registered, and in
**Blend membership by epoch** for the epochs it earns points in. A node that misses an epoch's activity
message is left out of that epoch's membership and earns nothing for it.

### The node's key and auto-claim

Joining, cashing out and payout codes need the node's Blend signing key, and Uplink never has it:
it stays with the Blockchain app, which asks the user each time, showing what it would sign. Uplink
asks through Basecamp's intents, and passes the answer to uplink_module, which checks it:

| Step | Intent | The Blockchain app answers | Uplink then |
|---|---|---|---|
| Join | `node.sign_referral_enrolment` (`program`, `participant_account`, `referrer`) | the node's signature over the enrolment | `enrol_with_signature` |
| Cash out | `node.referral_receipt_secret` | the node's receipt secret | `receipts_with_secret` (earlier cash-outs this Basecamp doesn't know of), then `cash_out_with_secret` |
| Find earlier cash-outs | `node.referral_receipt_secret` | the node's receipt secret | `receipts_with_secret` |
| Payout code | `node.sign_referral_payout` (`program_account`, `blinding_factor`) | the node's signature over the receipt's opening | `payout_code_with_signature` |

The Blockchain app fills in its own node and names it in its answer; an answer for another node
than the one running is refused, and declining puts the step back where it was. Auto-claim (on by
default, chosen on the last onboarding step, changeable in About → Settings) collects whenever
something is claimable; it needs no key. It lives in Uplink's `uplink.ini`.

Uplink's module remembers each cash-out it makes. After Basecamp's data is reset (a new computer,
a fresh install, cleared data) it knows none of the earlier ones, and their payout codes are out of
reach until they're found on chain with the receipt secret: from **Find earlier cash-outs** in the
past cash-outs dialog (the overview's **Past cash-outs** opens it when none are known), or on the
next cash-out.

### Invitation codes

uplink_module's: `uplink-invite-v1:` + base64url (no padding) of the inviter's node, then its
participant's nullifier and viewing public keys. Uplink passes them through untouched. The `uplink`
CLI's invitation JSON is accepted too.

### Build and test

```bash
nix build          # the plugin
nix flake check    # unit tests, backend tests and UI tests
```

Three layers, all on the mock backend:

- **Unit and backend tests** (`tests/*.cpp`, `checks.<system>.unit-tests`). The mock's rules,
  points arithmetic, uplink_module's replies as the real service reads them; and `UplinkBackend`
  itself, driven as QML drives it: terms, joining, the node app's answers (and refusals), auto-claim, the wallet closing or
  switching, cashing out, getting a past code again. The real services are stand-ins there
  (`tests/fakes/`).
- **UI tests** (`tests/*.mjs`, `checks.<system>.integration-test`). The standalone app, headless,
  driven through the real QML with [logos-qt-mcp](https://github.com/logos-co/logos-qt-mcp). Each
  file is one launch with its own environment: `ui-journey.mjs` goes from the welcome page to a
  payout code; `ui-not-ready.mjs` has no wallet and a stopped node. Against a running app:
  `nix build .#test-framework -o result-mcp`, start the app with the file's environment, then
  `node tests/ui-journey.mjs`.
- **Doc-test** (`doctests/`). The same journey with screenshots, rendered to Markdown by
  [logos-doctest](https://github.com/logos-co/logos-doctest): `doctests/run.sh` (Linux) runs it
  against this checkout and writes `doctests/outputs/`.
