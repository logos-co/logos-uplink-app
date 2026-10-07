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
- A LEZ wallet. Your referral identity lives in it and can be restored from its recovery phrase.

## Dependencies

| Piece | Provides | Status |
|---|---|---|
| [LEZ referral program](https://github.com/logos-blockchain/logos-execution-zone/pull/896) | Registration, points, claims, cash-out | Open draft PR |
| `lez_core` ([logos-execution-zone-module](https://github.com/logos-blockchain/logos-execution-zone-module)) | Access to the referral program from Basecamp | Needs referral calls once the PR lands |
| `blockchain_module` ([logos-blockchain-module](https://github.com/logos-blockchain/logos-blockchain-module)) | Node status, Blend core role, signing with the node key | Status available; signing tracked in [#108](https://github.com/logos-blockchain/logos-blockchain-module/issues/108) |
