import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { execFileSync } from "node:child_process";

const unpackedDir = path.resolve("dist/npm/unpacked/package");

for (const file of [
  "index.js",
  "index.cjs",
  "index.umd.js",
  "react.js",
  "react.cjs",
  "arguments.js",
  "filetypes.js",
  "network.js",
  "index.d.ts",
  "react.d.ts",
  "package.json"
]) {
  assert.ok(fs.existsSync(path.join(unpackedDir, file)), `Missing packed file: ${file}`);
}

execFileSync(process.execPath, ["interfaces/js/test/package/runtime-check.mjs"], {
  stdio: "inherit"
});

const consumerDir = fs.mkdtempSync(path.join(os.tmpdir(), "infomap-package-check-"));

fs.mkdirSync(path.join(consumerDir, "node_modules", "@mapequation"), { recursive: true });
fs.symlinkSync(
  unpackedDir,
  path.join(consumerDir, "node_modules", "@mapequation", "infomap"),
  "dir"
);
fs.symlinkSync(
  path.resolve("node_modules", "react"),
  path.join(consumerDir, "node_modules", "react"),
  "dir"
);
fs.symlinkSync(
  path.resolve("node_modules", "@types"),
  path.join(consumerDir, "node_modules", "@types"),
  "dir"
);
fs.copyFileSync(
  path.resolve("interfaces/js/test/package/consumer.ts"),
  path.join(consumerDir, "consumer.ts")
);

execFileSync(
  path.resolve("node_modules", ".bin", "tsc"),
  [
    "--module",
    "esnext",
    "--moduleResolution",
    "bundler",
    "--target",
    "es2020",
    "--noEmit",
    "--skipLibCheck",
    "consumer.ts",
  ],
  {
    cwd: consumerDir,
    stdio: "inherit"
  }
);
