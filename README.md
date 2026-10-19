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
| `uplink_module` | The programme for Basecamp: terms, enrolment (signed with the node's Blend key, read from its keystore), claims, cash-out, payout codes | [logos-blockchain/uplink-module](https://github.com/logos-blockchain/uplink-module) |
| `lez_core` ([logos-execution-zone-module](https://github.com/logos-blockchain/logos-execution-zone-module)) | The wallet: keys, sync, private transactions | Needs [#69](https://github.com/logos-blockchain/logos-execution-zone-module/pull/69) (`send_generic_private_transaction_with_accounts`) |
| `blockchain_module` ([logos-blockchain-module](https://github.com/logos-blockchain/logos-blockchain-module)) | Node status, Blend core role | Status available; the node's `provider_id` waits on [#108](https://github.com/logos-blockchain/logos-blockchain-module/issues/108). Until then the keystore the user chooses names the node |

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
scripts/devnet-keystore.sh  a keystore.yaml for a devnet node
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
the module's own choice (`configure` or `REFERRAL_PROGRAM_ACCOUNT`). The mock ignores both.

```bash
nix run .                              # everything real
UPLINK_BACKEND=mock nix run .          # everything mock
UPLINK_LEZ=mock nix run .              # real node, mock referral programme
UPLINK_NODE=mock nix run .             # mock node, real referral programme
UPLINK_NODE=mock:not_core nix run .    # mock node that is not a core node
```

The mock referral programme follows the program's rules: points accrue (a claim pays every epoch a
referral was active, and your own node was too, since it was last settled), cash-out takes the whole
balance, and a keystore for another node is refused. Its node's keystore is chosen for you
(`MockReferralService::keystorePath()`; one with `other-node` in the path holds another node's key).

It settles operations on the next poll (5 s), seeds three children when you
register, and publishes a new epoch every 30 s (`UPLINK_MOCK_EPOCH_MS` changes that). The only
registered node at start is `MockReferralService::inviterNode()`.

### Testing against the uplink devnet

[logos-blockchain/uplink](https://github.com/logos-blockchain/uplink)'s devnet (`just devnet up`)
runs four nodes, the LEZ sequencer and indexer, and the oracle in Docker. Basecamp's own node
can't join it, so for testing Uplink reads one of its nodes over HTTP as "your node"; everything
else is real: uplink_module, lez_core and the devnet's program.

1. In the uplink checkout: `just devnet up`, then make node 1 a Blend node with
   `just devnet blend-join 1`. It joins the membership two epochs (80 s) later.
2. Write a keystore for node 1: `scripts/devnet-keystore.sh <uplink>/devnet/state/config/node-1`.
   It prints the file and node 1's provider ID.
3. Note the devnet's program account: `cat <uplink>/devnet/state/program`. It changes on
   every `just devnet reset`. `metadata.json` ships no program (none is deployed on a public
   network yet), so Uplink leaves the choice to the module, which reads `UPLINK_PROGRAM`.
4. In the LEZ Wallet app, use the sequencer at `http://127.0.0.1:3040`.
5. Start Uplink against node 1:
   ```bash
   UPLINK_PROGRAM=<program account> UPLINK_NODE=devnet:http://127.0.0.1:18081 \
     UPLINK_NODE_ID=<node 1's provider ID> nix run .
   ```
   Nodes 0–3 serve their API on ports 18080–18083. On joining, choose the keystore from step 2.

Points need node 1 and the nodes under it active in a published epoch: the oracle publishes the
Blend membership node 0 sees (`just devnet blend-status`), and `just devnet dashboard` shows it.

### The Blend key and auto-claim

Joining, cashing out and payout codes are signed with the node's Blend signing key (`BlendSigning`
in its `keystore.yaml`; the UI calls it the Blend key, not the "node key", which the Blockchain app
uses for the network identity). Each of them first opens a dialog saying what the key signs and
which `keystore.yaml` it's read from, and runs
only once the user confirms; the first time, the user picks the file there (the picker opens at
Basecamp's data folder; About → Settings changes it too). Uplink keeps the path, never the key,
and uplink_module reads only that key, only for the action the user confirmed.
A keystore for another node than the one running is refused. Auto-claim (on by default, chosen on the
last onboarding step, changeable in About → Settings) collects whenever something is claimable;
it needs no key. Both settings live in Uplink's `uplink.ini`.

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
  itself, driven as QML drives it: terms, the keystore, joining, auto-claim, the wallet closing or
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
