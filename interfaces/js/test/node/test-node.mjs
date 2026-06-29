// Fast smoke test for the Emscripten Node artifacts (`make build-node`),
// exercising both at the raw module level before the heavier npm package build
// (the packaged ./node API and `infomap` bin are verified by `make test-js`):
//
//   build/js/infomap.node.mjs  MEMFS library  -> in-memory run, capturable output
//   build/js/infomap.cli.mjs   NODERAWFS bin   -> reads/writes real files
import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import createCliModule from "../../../../build/js/infomap.cli.mjs";
import createLibModule from "../../../../build/js/infomap.node.mjs";

const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "../../../..",
);
const network = fs.readFileSync(
  path.join(repoRoot, "examples/networks/twotriangles.net"),
  "utf8",
);

// 1. Library artifact (MEMFS): stage in memory, run, read results back, and
//    confirm Infomap's output is captured rather than printed.
{
  const stdout = [];
  const Module = await createLibModule({
    noInitialRun: true,
    print: (line) => stdout.push(line),
    printErr: (line) => stdout.push(line),
  });
  Module.FS.mkdir("/work");
  Module.FS.writeFile("/work/network.net", network);
  const status = Module.callMain([
    "/work/network.net",
    "/work",
    "-o",
    "tree,json",
    "-2",
  ]);
  assert.ok(
    status === 0 || status === undefined,
    `lib artifact exited with status ${status}`,
  );
  const tree = Module.FS.readFile("/work/network.tree", { encoding: "utf8" });
  const json = JSON.parse(
    Module.FS.readFile("/work/network.json", { encoding: "utf8" }),
  );
  assert.ok(tree.length > 0, "lib artifact produced an empty tree");
  assert.ok(
    Array.isArray(json.nodes) && json.nodes.length > 0,
    "lib json nodes",
  );
  assert.ok(stdout.length > 0, "lib artifact output should be capturable");
  console.log(
    `✓ Library artifact (MEMFS) ran in memory (${json.nodes.length} nodes, codelength ${json.codelength})`,
  );
}

// 2. Bin artifact (NODERAWFS): write to a real directory like the native CLI.
{
  const outDir = fs.mkdtempSync(path.join(os.tmpdir(), "infomap-node-cli-"));
  try {
    const inputPath = path.join(outDir, "twotriangles.net");
    fs.writeFileSync(inputPath, network);
    const Module = await createCliModule({ noInitialRun: true });
    const status = Module.callMain([inputPath, outDir, "--tree", "--clu"]);
    assert.ok(
      status === 0 || status === undefined,
      `cli artifact exited with status ${status}`,
    );
    const tree = path.join(outDir, "twotriangles.tree");
    const clu = path.join(outDir, "twotriangles.clu");
    assert.ok(fs.existsSync(tree) && fs.statSync(tree).size > 0, "cli .tree");
    assert.ok(fs.existsSync(clu), "cli .clu");
    console.log("✓ Bin artifact (NODERAWFS) wrote twotriangles.tree and .clu");
  } finally {
    fs.rmSync(outDir, { recursive: true, force: true });
  }
}

console.log("test-node: all checks passed");
