#!/usr/bin/env node
// The overview's notices: a node that isn't a Blend core node says so, and earlier epochs the
// module can't count yet (no LEZ indexer) are explained, with a way to start the indexer.
//
// Usage:
//   node tests/ui-overview-notices.mjs                 # against a running app (see the env below)
//   node tests/ui-overview-notices.mjs --ci <binary>   # launch, test, kill (what `nix flake check` does)
//
// Requires: nix build .#test-framework -o result-mcp
//
// Self-contained on purpose: the module builder copies each tests/*.mjs into the store alone.

import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

process.env.UPLINK_LEZ = "mock:history_needed";   // 3 epochs, at least 4 points, uncounted
process.env.UPLINK_NODE = "mock:not_core";
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

  // Not being a Blend core node doesn't stop joining; it stops earning.
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

// ---- The notices ----------------------------------------------------------------------------

test("overview: a node that isn't a Blend core node is told it earns nothing", async (app) => {
  await waitProp(app, "uplink.notCoreBanner", "visible", is(true));
  await app.expectTexts(["(not a Blend core node)"]);
});

test("overview: epochs the module can't count yet are explained, with a way to fix it", async (app) => {
  await waitProp(app, "uplink.historyBanner", "visible", is(true), { timeout: 15000 });
  await waitProp(app, "uplink.historyBanner", "message", (m) => m.includes("3 earlier epochs") && m.includes("At least 4 points"));
  await waitProp(app, "uplink.openExplorerButton", "visible", is(true));
});

run();
