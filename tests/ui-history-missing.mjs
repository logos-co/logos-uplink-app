#!/usr/bin/env node
// An indexer that has caught up without the epochs Uplink needs. The banner says to check it,
// not to start one, guesses no cause, and says nothing about points when nothing is known
// about them ("at least 0 points" was shown).
//
// Usage:
//   node tests/ui-history-missing.mjs                 # against a running app (see the env below)
//   node tests/ui-history-missing.mjs --ci <binary>   # launch, test, kill (what `nix flake check` does)
//
// Requires: nix build .#test-framework -o result-mcp
//
// Self-contained on purpose: the module builder copies each tests/*.mjs into the store alone.

import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

process.env.UPLINK_LEZ = "mock:history_missing";   // 3 epochs uncounted, nothing known about their points
process.env.UPLINK_NODE = "mock";
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

// ---- Join as a root, to reach the overview ------------------------------------------------

test("join: as a root, through terms and the Ready step", async (app) => {
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

  await waitProp(app, "uplink.ReadyStep", "ready", is(true), { timeout: 15000 });
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.JoinStep", "visible", is(true));
  await click(app, "uplink.rootChoice");
  await waitProp(app, "uplink.onboardingAdvanceButton", "text", is("Join as root"));
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.DoneStep", "visible", is(true), { timeout: 20000 });
  await click(app, "uplink.onboardingAdvanceButton");
  await waitProp(app, "uplink.OverviewPage", "visible", is(true));
});

// ---- The banner ---------------------------------------------------------------------------

test("overview: a caught-up indexer without the epochs is to be checked, not started", async (app) => {
  await waitProp(app, "uplink.historyBanner", "visible", is(true), { timeout: 15000 });
  const message = await waitProp(app, "uplink.historyBanner", "message", (m) => m.includes("3 earlier epochs"));
  if (!message.includes("Check it in the LEZ Explorer app")) throw new Error(`doesn't say to check the indexer: ${message}`);
  if (message.includes("network")) throw new Error(`guesses a cause it can't know: ${message}`);
  if (message.includes("Start it")) throw new Error(`tells the user to start a running indexer: ${message}`);
  if (message.includes("points are waiting")) throw new Error(`speaks of points nobody knows about: ${message}`);
  await waitProp(app, "uplink.openExplorerButton", "visible", is(true));
});

run();
