#!/usr/bin/env node
// The wallet's points account joined with another node than the one Uplink runs beside. The
// overview says the running node isn't earning for it, and labels the account's node as such.
//
// Usage:
//   node tests/ui-other-node.mjs                 # against a running app (see the env below)
//   node tests/ui-other-node.mjs --ci <binary>   # launch, test, kill (what `nix flake check` does)
//
// Requires: nix build .#test-framework -o result-mcp
//
// Self-contained on purpose: the module builder copies each tests/*.mjs into the store alone.

import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

process.env.UPLINK_LEZ = "mock:already_joined_later";   // no wallet, then (10 s on) one already joined
process.env.UPLINK_NODE = "mock:other_node";   // not the node the mock's accounts join with
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

// ---- Start joining, then the wallet turns up ------------------------------------------------

test("onboarding: start joining while the wallet isn't open yet", async (app) => {
  await waitProp(app, "uplink.WelcomePage", "visible", is(true), { timeout: 30000 });
  await click(app, "uplink.joinButton");
  await waitProp(app, "uplink.TermsStep", "visible", is(true));
  const scroll = await find(app, "uplink.termsScroll", { visibleOnly: true });
  const { contentHeight, height } = await props(app, scroll);
  const res = await app.inspector.send("setProperty", { objectId: scroll, property: "contentY", value: contentHeight - height });
  if (res.error) throw new Error(res.error);
  await waitProp(app, "uplink.termsCheckbox", "enabled", is(true));
  await click(app, "uplink.termsCheckbox");
  await waitProp(app, "uplink.onboardingAdvanceButton", "enabled", is(true));
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.ReadyStep", "visible", is(true));
  if ((await prop(app, "uplink.ReadyStep", "ready")) !== false)
    throw new Error("ready before the wallet is open");
});

test("onboarding: the account turns up, and Finish lands on the overview", async (app) => {
  await waitProp(app, "uplink.DoneStep", "visible", is(true), { timeout: 30000 });
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.OverviewPage", "visible", is(true));
});

test("overview: an account of another node is flagged, and its row named for it", async (app) => {
  await waitProp(app, "uplink.otherNodeBanner", "visible", is(true), { timeout: 15000 });
  const message = await prop(app, "uplink.otherNodeBanner", "message");
  if (!message.includes("isn't earning")) throw new Error(`doesn't say the node isn't earning: ${message}`);
  await app.expectTexts(["Account's node"]);
});

run();
