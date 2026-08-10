import fs from "node:fs";
import path from "node:path";
import commonjs from "@rollup/plugin-commonjs";
import json from "@rollup/plugin-json";
import nodeResolve from "@rollup/plugin-node-resolve";
import esbuild from "rollup-plugin-esbuild";

const sourceRoot = path.resolve("interfaces/js/src");
const workerPath = path.resolve("build/js/infomap.worker.js");
const outputDir = path.resolve("dist/npm/package");

function workerSourcePlugin() {
  const workerModuleId = "\0infomap-worker-source";

  return {
    name: "infomap-worker-source",
    resolveId(source, importer) {
      if (!importer) {
        return null;
      }

      const resolved = path.resolve(path.dirname(importer), source);
      if (resolved === workerPath) {
        return workerModuleId;
      }

      return null;
    },
    load(id) {
      if (id !== workerModuleId) {
        return null;
      }

      const source = fs.readFileSync(workerPath, "utf8");
      return `export default ${JSON.stringify(source)};`;
    },
  };
}

const packageEntries = {
  index: path.join(sourceRoot, "index.ts"),
  arguments: path.join(sourceRoot, "arguments.ts"),
  filetypes: path.join(sourceRoot, "filetypes.ts"),
  network: path.join(sourceRoot, "network.ts"),
  react: path.join(sourceRoot, "react.ts"),
  result: path.join(sourceRoot, "result.ts"),
  node: path.join(sourceRoot, "node.ts"),
};

// The Emscripten Node artifacts (copied into the package by
// prepare_npm_package.py) stay external so node.js/cli.js import them at runtime
// rather than inlining the wasm twice.
const external = ["react", /infomap\.(node|cli)\.mjs$/];

const basePlugins = [
  workerSourcePlugin(),
  nodeResolve({
    browser: true,
    extensions: [".ts", ".js", ".json"],
  }),
  commonjs(),
  json(),
  // esbuild only transpiles (TS -> JS); it inherits `target` from the tsconfig.
  // Type-checking and .d.ts emit are handled separately by `tsc` in build:package.
  // This keeps the bundler free of the TypeScript compiler API, which TypeScript
  // 7 no longer ships (deferred to 7.1) and @rollup/plugin-typescript relied on.
  esbuild({
    tsconfig: "./interfaces/js/tsconfig.json",
    sourceMap: true,
    // cli.ts uses top-level await in its ES-module bin output, which is valid
    // there and is what TypeScript emitted before. esbuild otherwise gates it on
    // an ES2022 target, so allow it explicitly while keeping the ES2020 target.
    supported: { "top-level-await": true },
  }),
];

export default [
  {
    input: packageEntries,
    output: {
      dir: outputDir,
      entryFileNames: "[name].js",
      format: "es",
      sourcemap: true,
    },
    external,
    plugins: basePlugins,
  },
  {
    input: packageEntries,
    output: {
      dir: outputDir,
      entryFileNames: "[name].cjs",
      // Shared chunks must also be .cjs: a .js chunk holding CommonJS code can't
      // be require()'d in this "type": "module" package (Node treats .js as ESM).
      chunkFileNames: "[name]-[hash].cjs",
      format: "cjs",
      exports: "named",
      sourcemap: true,
      // Keep `await import("./infomap.node.mjs")` as a real dynamic import so the
      // CommonJS build can load the ES-module artifact instead of require()'ing it.
      dynamicImportInCjs: true,
    },
    external,
    plugins: basePlugins,
  },
  {
    // `infomap` bin: ESM only, with a shebang. Backed by the NODERAWFS artifact.
    input: path.join(sourceRoot, "cli.ts"),
    output: {
      file: path.join(outputDir, "cli.js"),
      format: "es",
      banner: "#!/usr/bin/env node",
      sourcemap: true,
    },
    external,
    plugins: basePlugins,
  },
  {
    input: path.join(sourceRoot, "index.ts"),
    output: {
      file: path.join(outputDir, "index.umd.js"),
      format: "umd",
      name: "infomap",
      exports: "named",
      sourcemap: true,
    },
    plugins: basePlugins,
  },
];
