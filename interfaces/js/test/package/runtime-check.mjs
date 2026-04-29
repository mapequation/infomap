import assert from "node:assert/strict";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { createRequire } from "node:module";

const packageDir = path.resolve("dist/npm/unpacked/package");
const esmEntry = pathToFileURL(path.join(packageDir, "index.js")).href;
const reactEntry = pathToFileURL(path.join(packageDir, "react.js")).href;
const require = createRequire(import.meta.url);

const esmModule = await import(esmEntry);
const reactModule = await import(reactEntry);
const cjsModule = require(path.join(packageDir, "index.cjs"));

assert.equal(typeof esmModule.default, "function");
assert.equal(typeof cjsModule.default, "function");
assert.equal(typeof reactModule.useInfomap, "function");
assert.equal(esmModule.default.__version__, cjsModule.default.__version__);
