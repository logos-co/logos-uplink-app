# Uplink

Basecamp app for the Logos node referral programme: node operators invite other node operators, and
earn points while the nodes they brought in do real work on the network.

> Status: early development. The programme's rules are not final, and the backend it needs is not
> released yet (see [Dependencies](#dependencies)).

## How the programme works

- **Join under someone.** You import the invitation from the person who referred you, then enrol.
  Enrolling binds your node to that referrer permanently. There are no referral codes: the bind is
  signed by your node's key. You can also enrol without a referrer and start your own tree.
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
| [LEZ referral program](https://github.com/logos-blockchain/logos-execution-zone/pull/896) | Registration, points, claims, cash-out | Open draft PR |
| `lez_core` ([logos-execution-zone-module](https://github.com/logos-blockchain/logos-execution-zone-module)) | Access to the referral program from Basecamp | Needs referral calls once the PR lands |
| `blockchain_module` ([logos-blockchain-module](https://github.com/logos-blockchain/logos-blockchain-module)) | Node status, Blend core role, signing with the node key | Status available; signing tracked in [#108](https://github.com/logos-blockchain/logos-blockchain-module/issues/108) |

## Development

A `ui_qml` module: a QML view plus a C++ backend (`UplinkBackend`), with the contract in
`src/uplink_ui.rep`.

### Layout

```
src/uplink_ui.rep           the contract the view binds to
src/UplinkBackend.*         app state: participant, operation refs, labels, points totals
src/interfaces/             interfaces
  ReferralService.h           the LEZ referral wallet facade (logos-execution-zone#896), 1:1
  NodeService.h               the local node: status, Blend role, signing
src/lez/                    ReferralService over lez_core
src/blockchain/             NodeService over blockchain_module
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
| `UPLINK_LEZ` | `mock`, `mock:no_wallet`, `real` | Referral side. `real` uses lez_core: the wallet check, the identity's label and address, and sync work today; referral calls fail until lez_core has them (needs #896 merged + wallet-ffi bindings + lez_core methods). `mock:no_wallet` makes the mock report that no wallet is open. |
| `UPLINK_NODE` | `mock`, `mock:<issue>`, `real` | Node side. `real` uses blockchain_module: status works today (`get_cryptarchia_info`, `get_chain_id`, `blend_info`); the node's `provider_id` waits on logos-blockchain-module#108. Signing is not done here: the node app signs on the user's approval, via the `node.sign_message` intent. `mock:<issue>` makes the mock node report `module`, `not_running`, `bootstrapping`, `not_core` or `no_peers`. |

```bash
nix run .                              # everything real
UPLINK_BACKEND=mock nix run .          # everything mock
UPLINK_LEZ=mock nix run .              # real node, mock referral programme
UPLINK_NODE=mock nix run .             # mock node, real referral programme
UPLINK_NODE=mock:not_core nix run .    # mock node that is not a core node
```

The mock referral programme follows #896 except for one assumption: points **accrue**. A claim pays
for every epoch a referral was active since you last claimed. #896 currently pays only the current
epoch, so epochs you don't claim in are lost.

It settles operations on the next poll (5 s), seeds three children when you
register, and publishes a new epoch every 30 s (`UPLINK_MOCK_EPOCH_MS` changes that). The only
registered node at start is `MockReferralService::inviterNode()`.

### Invitation codes

An invitation is `uplink-invite:` + base64url (no padding) of the JSON blob lez_core hands out
and takes back: `{"parent_node", "npk", "vpk"}`, logos-execution-zone#896's `Invitation`, keys in
hex. `src/InvitationCode.h` encodes and decodes it; a format change gets a new prefix.

### Build and test

```bash
nix build          # the plugin
nix flake check    # unit tests, backend tests and UI tests
```

Three layers, all on the mock backend:

- **Unit and backend tests** (`tests/*.cpp`, `checks.<system>.unit-tests`). The mock's rules, the
  activity log, points arithmetic, codes; and `UplinkBackend` itself, driven as QML drives it:
  joining, the node's signature, the wallet closing or switching, cashing out, getting a past
  code again. The real services are stand-ins there (`tests/fakes/`).
- **UI tests** (`tests/*.mjs`, `checks.<system>.integration-test`). The standalone app, headless,
  driven through the real QML with [logos-qt-mcp](https://github.com/logos-co/logos-qt-mcp). Each
  file is one launch with its own environment: `ui-journey.mjs` goes from the welcome page to a
  payout code; `ui-not-ready.mjs` has no wallet and a stopped node. Against a running app:
  `nix build .#test-framework -o result-mcp`, start the app with the file's environment, then
  `node tests/ui-journey.mjs`.
- **Doc-test** (`doctests/`). The same journey with screenshots, rendered to Markdown by
  [logos-doctest](https://github.com/logos-co/logos-doctest): `doctests/run.sh` (Linux) runs it
  against this checkout and writes `doctests/outputs/`.
