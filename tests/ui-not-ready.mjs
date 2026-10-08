#!/usr/bin/env node
// Onboarding when nothing is ready: the node isn't running and no LEZ wallet is open.
// The Ready step must say so, offer both apps, and not let the user continue.
//
// Usage:
//   node tests/ui-not-ready.mjs                 # against a running app (see the env below)
//   node tests/ui-not-ready.mjs --ci <binary>   # launch, test, kill (what `nix flake check` does)
//
// Requires: nix build .#test-framework -o result-mcp
//
// Self-contained on purpose: the module builder copies each tests/*.mjs into the store alone.

import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

process.env.UPLINK_LEZ = "mock:no_wallet";
process.env.UPLINK_NODE = "mock:not_running";
process.env.XDG_DATA_HOME = mkdtempSync(join(tmpdir(), "uplink-ui-"));

const root = process.env.LOGOS_QT_MCP || new URL("../result-mcp", import.meta.url).pathname;
const { test, run } = await import(resolve(root, "test-framework/framework.mjs"));

// ---- Helpers (as in ui-journey.mjs) -----------------------------------------------------

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

async function waitProp(app, name, property, check, { timeout = 10000, description } = {}) {
  let last;
  await app.waitFor(async () => {
    last = await prop(app, name, property);
    if (!check(last)) throw new Error("not yet");
  }, { timeout, interval: 300, description: description ?? `${name}.${property} (last: ${JSON.stringify(last)})` });
  return last;
}

const is = (expected) => (v) => v === expected;

// ---- Tests ------------------------------------------------------------------------------------

test("not ready: get through the terms to the Ready step", async (app) => {
  await waitProp(app, "uplink.WelcomePage", "visible", is(true), { timeout: 30000 });
  await click(app, "uplink.joinButton");
  await waitProp(app, "uplink.TermsStep", "visible", is(true));
  const scroll = await find(app, "uplink.termsScroll", { visibleOnly: true });
  const { contentHeight, height } = await props(app, scroll);
  await app.inspector.send("setProperty", { objectId: scroll, property: "contentY", value: contentHeight - height });
  await waitProp(app, "uplink.termsCheckbox", "enabled", is(true));
  await click(app, "uplink.termsCheckbox");
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(true));
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.ReadyStep", "visible", is(true));
});

test("not ready: both problems are named, with a way to fix each", async (app) => {
  await app.expectTexts(["YOUR NODE (NOT DETECTED)", "YOUR LEZ WALLET (NOT OPEN)"]);
  await waitProp(app, "uplink.openBlockchainButton", "visible", is(true));
  await waitProp(app, "uplink.openWalletButton", "visible", is(true));
});

test("not ready: the user can't continue, and is told why", async (app) => {
  await waitProp(app, "uplink.ReadyStep", "ready", is(false));
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(false));
  await waitProp(app, "uplink.onboardingAdvanceHint", "text", is("Your node and LEZ wallet need to be ready."));
});

test("not ready: Back returns to the terms, still accepted", async (app) => {
  await click(app, "uplink.onboardingBackButton");
  await waitProp(app, "uplink.TermsStep", "visible", is(true));
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(true));
});

run();
