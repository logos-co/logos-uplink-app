#!/usr/bin/env node
// The whole Uplink journey against the mock backend, driven through the real QML:
// welcome, terms, ready, join under an invitation, the overview, inviting, cashing out,
// and getting a past cash-out's code again. The tests run in order on one app.
//
// Usage:
//   node tests/ui-journey.mjs                 # against a running app (started with UPLINK_BACKEND=mock)
//   node tests/ui-journey.mjs --ci <binary>   # launch, test, kill (what `nix flake check` does)
//   node tests/ui-journey.mjs <substring>     # filter tests by name
//
// Requires: nix build .#test-framework -o result-mcp
//
// Self-contained on purpose: the module builder copies each tests/*.mjs into the store alone.

import { createHash } from "node:crypto";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

// Read by the app launched in --ci mode. Epochs of 15 s: the mock's own node is active in
// epochs 2 and 3 (15-45 s after launch), which is when the cash-out below runs.
process.env.UPLINK_BACKEND = "mock";
process.env.UPLINK_MOCK_EPOCH_MS = "15000";
process.env.XDG_DATA_HOME = mkdtempSync(join(tmpdir(), "uplink-ui-"));   // labels and activity

const root = process.env.LOGOS_QT_MCP || new URL("../result-mcp", import.meta.url).pathname;
const { test, run } = await import(resolve(root, "test-framework/framework.mjs"));

// ---- Helpers ------------------------------------------------------------------------

// The first object named `name`, or the first visible one when there are several
// (a closed dialog's contents are still in the tree).
async function find(app, name, { visibleOnly = false } = {}) {
  const res = await app.findByProperty("objectName", name);
  if (res.error || !res.matches?.length) throw new Error(`no object named "${name}"`);
  if (!visibleOnly) return res.matches[0].id;
  for (const m of res.matches)
    if ((await props(app, m.id)).visible === true) return m.id;
  throw new Error(`no visible object named "${name}"`);
}

async function props(app, id) {
  const res = await app.getProperties(id);
  if (res.error) throw new Error(`getProperties: ${res.error}`);
  return Object.fromEntries(res.properties.map((p) => [p.name, p.value]));
}

async function prop(app, name, property) {
  const value = (await props(app, await find(app, name)))[property];
  if (value === undefined) throw new Error(`"${name}" has no property "${property}"`);
  return value;
}

async function click(app, name) {
  const id = await find(app, name, { visibleOnly: true });
  const res = await app.inspector.send("click", { objectId: id });
  if (res.error) throw new Error(`click "${name}": ${res.error}`);
}

async function set(app, name, property, value) {
  const res = await app.inspector.send("setProperty", { objectId: await find(app, name), property, value });
  if (res.error) throw new Error(`set ${name}.${property}: ${res.error}`);
}

async function waitProp(app, name, property, check, { timeout = 10000, description } = {}) {
  let last;
  await app.waitFor(async () => {
    last = await prop(app, name, property);
    if (!check(last)) throw new Error("not yet");
  }, { timeout, interval: 300, description: description ?? `${name}.${property} (last: ${JSON.stringify(last)})` });
  return last;
}

const is = (expected) => (v) => v === expected;

// What a mock inviter hands out (MockReferralService::invitationFor), as a shareable code.
function invitationCode(node) {
  const hex = (s) => createHash("sha256").update(s).digest("hex");
  const blob = JSON.stringify({ npk: hex(`npk:${node}`), parent_node: node, vpk: hex(`vpk:${node}`) });
  return "uplink-invite:" + Buffer.from(blob).toString("base64url");
}
const mockInviter = createHash("sha256").update("mock-inviter").digest("hex");

let payoutCode = "";

// ---- Welcome and terms -------------------------------------------------------------------

test("welcome: a new user lands on the welcome page", async (app) => {
  await waitProp(app, "uplink.WelcomePage", "visible", is(true), { timeout: 30000 });
  await app.expectTexts(["Logos Uplink", "Join Uplink"]);
});

test("terms: can't be accepted before reading to the end", async (app) => {
  await click(app, "uplink.joinButton");
  await waitProp(app, "uplink.TermsStep", "visible", is(true));
  if ((await prop(app, "uplink.termsCheckbox", "enabled")) !== false)
    throw new Error("the terms checkbox is enabled before the terms were read");
  if ((await prop(app, "uplink.onboardingAdvanceButton", "enabled")) !== false)
    throw new Error("Continue is enabled before the terms were accepted");
});

test("terms: reading to the end and ticking the box allows continuing", async (app) => {
  const scroll = await find(app, "uplink.termsScroll", { visibleOnly: true });
  const { contentHeight, height } = await props(app, scroll);
  const res = await app.inspector.send("setProperty", { objectId: scroll, property: "contentY", value: contentHeight - height });
  if (res.error) throw new Error(res.error);
  await waitProp(app, "uplink.termsCheckbox", "enabled", is(true));
  await click(app, "uplink.termsCheckbox");
  await waitProp(app, "uplink.termsCheckbox", "checked", is(true));
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(true));
  await click(app, "uplink.onboardingAdvanceButton");
});

// ---- Ready and join --------------------------------------------------------------------

test("ready: the mock node and wallet are detected", async (app) => {
  await waitProp(app, "uplink.ReadyStep", "visible", is(true));
  await waitProp(app, "uplink.ReadyStep", "ready", is(true), { timeout: 15000 });
  await app.expectTexts(["YOUR NODE (DETECTED)", "YOUR LEZ WALLET (OPEN)"]);
  await click(app, "uplink.onboardingAdvanceButton");
});

test("join: a code that isn't an invitation is explained", async (app) => {
  await waitProp(app, "uplink.JoinStep", "visible", is(true));
  await set(app, "uplink.invitationInput", "text", "hello");
  await waitProp(app, "uplink.invitationNote", "text", is("That doesn’t look like an invitation code."));
  if ((await prop(app, "uplink.onboardingAdvanceButton", "enabled")) !== false)
    throw new Error("joining is allowed with an invalid invitation");
});

test("join: an inviter who hasn't joined is explained", async (app) => {
  await set(app, "uplink.invitationInput", "text", invitationCode("e".repeat(64)));
  await waitProp(app, "uplink.invitationNote", "text", is("This inviter hasn’t joined the program yet."));
});

test("join: a good invitation enables joining under it", async (app) => {
  await set(app, "uplink.invitationInput", "text", invitationCode(mockInviter));
  await waitProp(app, "uplink.invitationNote", "text", is(""));
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(true));
  await waitProp(app, "uplink.onboardingAdvanceButton", "text", is("Import invitation & sign"));
});

test("join: choosing my own tree changes the action", async (app) => {
  await click(app, "uplink.rootChoice");
  await waitProp(app, "uplink.onboardingAdvanceButton", "text", is("Join as root"));
  await click(app, "uplink.inviteChoice");
  await waitProp(app, "uplink.onboardingAdvanceButton", "text", is("Import invitation & sign"));
});

test("join: the node signs, the join lands, and Done shows the points account", async (app) => {
  await click(app, "uplink.onboardingAdvanceButton");
  // The registration settles on the backend's next poll (every 5 s).
  await waitProp(app, "uplink.DoneStep", "visible", is(true), { timeout: 20000 });
  await app.expectTexts(["UPLINK POINTS ACCOUNT"]);
  await waitProp(app, "uplink.pointsAccount", "visible", is(true));
});

// ---- Overview ------------------------------------------------------------------------------

test("overview: Finish opens the overview with no points yet", async (app) => {
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.OverviewPage", "visible", is(true));
  await waitProp(app, "uplink.pointsBalance", "text", is("0 points"));
  if ((await prop(app, "uplink.cashedOut", "visible")) !== false)
    throw new Error("'cashed out so far' shows before any cash-out");
});

test("overview: the referrals the mock seeds are listed", async (app) => {
  await waitProp(app, "uplink.noReferrals", "visible", is(false), { timeout: 15000 });
});

test("invite: the dialog shows this node's invitation code", async (app) => {
  await click(app, "uplink.invitePeerButton");
  await waitProp(app, "uplink.InviteDialog", "opened", is(true));
  await waitProp(app, "uplink.invitationCode", "text", (t) => t.startsWith("uplink-invite:"));
  await click(app, "uplink.inviteCloseButton");
  await waitProp(app, "uplink.InviteDialog", "visible", is(false));
});

// ---- Cashing out ------------------------------------------------------------------------------

test("claim: points arrive once an epoch with active referrals is published", async (app) => {
  await waitProp(app, "uplink.pointsBalance", "text", (t) => t !== "0 points", { timeout: 40000 });
});

test("claim: cashing out collects, cashes out and shows a signed payout code", async (app) => {
  await click(app, "uplink.claimRewardsButton");
  await waitProp(app, "uplink.ClaimDialog", "opened", is(true));
  await waitProp(app, "uplink.cashOutButton", "enabled", is(true));
  await click(app, "uplink.cashOutButton");
  // Collect, then cash out: each settles on a poll.
  payoutCode = await waitProp(app, "uplink.payoutCode", "text", (t) => t.startsWith("uplink-payout-v2:"), { timeout: 30000 });
  await click(app, "uplink.claimDoneButton");
  await waitProp(app, "uplink.ClaimDialog", "visible", is(false));
});

test("claim: the balance is spent and the cash-out is counted", async (app) => {
  await waitProp(app, "uplink.cashedOut", "visible", is(true), { timeout: 10000 });
  await waitProp(app, "uplink.cashedOut", "text", (t) => /^[1-9]\d* cashed out so far$/.test(t));
});

test("cash-outs: a past cash-out's code can be got again, and it's the same code", async (app) => {
  await click(app, "uplink.cashedOut");
  await waitProp(app, "uplink.CashOutsDialog", "opened", is(true));
  await app.expectTexts(["Cash-out 1"]);
  await click(app, "uplink.cashOutCodeButton");
  await waitProp(app, "uplink.CashOutsDialog", "visible", is(false));
  await waitProp(app, "uplink.ClaimDialog", "opened", is(true));
  await waitProp(app, "uplink.payoutCode", "text", is(payoutCode), { timeout: 10000 });
  await click(app, "uplink.claimDoneButton");
});

run();
