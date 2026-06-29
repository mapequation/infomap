[![CI](https://github.com/mapequation/infomap/actions/workflows/ci.yml/badge.svg)](https://github.com/mapequation/infomap/actions/workflows/ci.yml)
[![Release](https://github.com/mapequation/infomap/actions/workflows/release.yml/badge.svg)](https://github.com/mapequation/infomap/actions/workflows/release.yml)

# @mapequation/infomap

`@mapequation/infomap` packages Infomap, compiled to WebAssembly with
Emscripten. It ships a browser web worker, a Node.js module
(`@mapequation/infomap/node`), and an `infomap` command line tool.

Infomap is a network clustering algorithm based on the
[Map equation](https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation).
The package is used in [Infomap Online](https://www.mapequation.org/infomap/).

## Install

```bash
npm install @mapequation/infomap
```

## Use with ES modules

```js
import Infomap from "@mapequation/infomap";

const network = `#source target [weight]
0 1
0 2
0 3
1 0
1 2
2 1
2 0
3 0
3 4
3 5
4 3
4 5
5 4
5 3`;

const infomap = new Infomap()
  .on("data", (data) => console.log(data))
  .on("error", (error) => console.warn(error))
  .on("finished", (data) => console.log(data));

infomap.run({
  network,
  args: {
    twoLevel: true,
  },
});
```

React users can import the hook entrypoint directly from the main package:

```js
import { useInfomap } from "@mapequation/infomap/react";
```

The hook exposes `run`, `runAsync`, `running`, `progress`, `output`,
`outputText`, `clearOutput`, and `on`. It keeps `running` set until the worker
finishes or errors, and it terminates active workers when the component
unmounts. Console output collection is opt-in.

```js
const { outputText, run, running } = useInfomap(
  { silent: true },
  {
    collectOutput: true,
    events: {
      finished: (result) => console.log(result),
      error: (message) => console.warn(message),
    },
  },
);

run({ network, args: { clu: true, tree: true } });
```

Result helpers are available from `@mapequation/infomap/result` for common UI
tasks such as metadata display and file downloads:

```js
import { getResultFiles, getResultMetadata } from "@mapequation/infomap/result";

const metadata = getResultMetadata(result);
const files = getResultFiles(result, "network");
```

## Use from a CDN

With JSDelivr, the package is available as `window.infomap.default`.

```html
<!doctype html>
<html>
  <head>
    <script src="https://cdn.jsdelivr.net/npm/@mapequation/infomap@latest/index.umd.js"></script>
  </head>
  <body>
    <script>
      const Infomap = window.infomap.default;
      const network = "#--- same network as above ---";

      const infomap = new Infomap()
        .on("data", (data) => console.log(data))
        .on("error", (error) => console.warn(error))
        .on("finished", (data) => console.log(data));

      infomap.run({
        network,
        args: {
          twoLevel: true,
        },
      });
    </script>
  </body>
</html>
```

## Use in Node.js

The `@mapequation/infomap/node` entrypoint runs Infomap in Node.js. It exposes
an async `run(network, options)` that resolves to the output files, keyed by
format (the default `@mapequation/infomap` entrypoint targets the browser and
relies on worker APIs that are unavailable in Node).

```js
import { run } from "@mapequation/infomap/node";

const network = `#source target
0 1
0 2
0 3
1 2
3 4
3 5
4 5`;

const result = await run(network, { args: ["-o", "tree,json", "-2"] });

console.log(result.json.codelength);
console.log(result.tree); // .tree file contents as a string
```

`options.args` accepts a string or an array of Infomap CLI options, and the
result contains only the formats Infomap produced. The network is processed in
memory and Infomap's console output is suppressed. The same entrypoint works
with CommonJS:

```js
const { run } = require("@mapequation/infomap/node");
```

## Command line

Installing the package provides an `infomap` command that behaves like the
native binary, reading and writing real files (`network_file out_directory
[options]`):

```bash
npx @mapequation/infomap network.net . --tree --clu
npx @mapequation/infomap --help
```

## Package notes

- `@mapequation/infomap/react` is the supported React entrypoint.
- `@mapequation/infomap/node` is the Node.js entrypoint; the default
  `@mapequation/infomap` entrypoint is browser-only.

## More information

- Main docs: [mapequation.org/infomap](https://www.mapequation.org/infomap/)
- Issues: [github.com/mapequation/infomap/issues](https://github.com/mapequation/infomap/issues)

## Authors

Daniel Edler, Anton Holmgren, Martin Rosvall

Contact details are available at
[mapequation.org/about.html](https://www.mapequation.org/about.html).

## Terms of use

Infomap is released under a dual licence.

The code is available under the GNU General Public License version 3 or any
later version; see `LICENSE_GPLv3.txt`.
For a non-copyleft license, please contact us.
