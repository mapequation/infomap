import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import { createRequire } from "node:module";
import os from "node:os";
import path from "node:path";
import { pathToFileURL } from "node:url";

const packageDir = path.resolve("dist/npm/unpacked/package");
const esmEntry = pathToFileURL(path.join(packageDir, "index.js")).href;
const reactEntry = pathToFileURL(path.join(packageDir, "react.js")).href;
const resultEntry = pathToFileURL(path.join(packageDir, "result.js")).href;
const nodeEntry = pathToFileURL(path.join(packageDir, "node.js")).href;
const require = createRequire(import.meta.url);

const esmModule = await import(esmEntry);
const reactModule = await import(reactEntry);
const resultModule = await import(resultEntry);
const nodeModule = await import(nodeEntry);
const cjsModule = require(path.join(packageDir, "index.cjs"));
const resultCjsModule = require(path.join(packageDir, "result.cjs"));
const nodeCjsModule = require(path.join(packageDir, "node.cjs"));

assert.equal(typeof esmModule.default, "function");
assert.equal(typeof cjsModule.default, "function");
assert.equal(typeof reactModule.useInfomap, "function");
assert.equal(typeof resultModule.getResultFiles, "function");
assert.equal(typeof resultCjsModule.getResultFiles, "function");
assert.equal(esmModule.default.__version__, cjsModule.default.__version__);
assert.equal(typeof nodeModule.run, "function");
assert.equal(typeof nodeCjsModule.run, "function");

// Exercise the Node library (ESM and CJS) end to end against the real wasm.
const network = "*Vertices 4\n*Edges\n1 2\n2 3\n3 1\n3 4\n";
for (const { label, run } of [
  { label: "esm", run: nodeModule.run },
  { label: "cjs", run: nodeCjsModule.run },
]) {
  const result = await run(network, { args: ["-o", "tree,json", "-2"] });
  assert.equal(typeof result.tree, "string", `${label} run produced no tree`);
  assert.ok(result.tree.length > 0, `${label} run tree is empty`);
  assert.ok(
    result.json &&
      Array.isArray(result.json.nodes) &&
      result.json.nodes.length > 0,
    `${label} run produced no json nodes`,
  );
}

// Exercise the `infomap` bin on real files, like the native binary.
const binPath = path.join(packageDir, "cli.js");
const binDir = fs.mkdtempSync(path.join(os.tmpdir(), "infomap-bin-check-"));
try {
  const inputPath = path.join(binDir, "network.net");
  fs.writeFileSync(inputPath, network);
  execFileSync(process.execPath, [binPath, inputPath, binDir, "--tree"], {
    stdio: "inherit",
  });
  const treePath = path.join(binDir, "network.tree");
  assert.ok(fs.existsSync(treePath), "bin did not produce network.tree");
  assert.ok(fs.statSync(treePath).size > 0, "bin network.tree is empty");
} finally {
  fs.rmSync(binDir, { recursive: true, force: true });
}
