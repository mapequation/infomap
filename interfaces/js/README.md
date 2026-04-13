[![CI](https://github.com/mapequation/infomap/actions/workflows/ci.yml/badge.svg)](https://github.com/mapequation/infomap/actions/workflows/ci.yml)
[![Release](https://github.com/mapequation/infomap/actions/workflows/release.yml/badge.svg)](https://github.com/mapequation/infomap/actions/workflows/release.yml)

# @mapequation/infomap

`@mapequation/infomap` packages Infomap as a browser web worker built with
Emscripten.

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

## Structured run events

The package can emit structured run events in addition to the default text
output. Set `logFormat` to `jsonl` or `both` to enable it.

```js
import Infomap from "@mapequation/infomap";

const infomap = new Infomap()
  .on("event", (event) => {
    if (event.type === "summary") {
      console.log("codelength", event.codelength);
    }
  })
  .on("jsonl", (line) => console.log(line))
  .on("finished", (result) => console.log(result));

infomap.run({
  network: "#source target\n1 2\n2 1\n",
  args: "--tree --clu --silent",
  logFormat: "both",
});
```

Supported structured event types:

- `run_started`
- `log_line`
- `trial_started`
- `trial_finished`
- `summary`
- `partition_result`
- `warning`
- `run_failed`
- `run_finished`

`logFormat: "text"` remains the default and preserves the existing `data`
callback behavior.

React users can import the hook entrypoint directly from the main package:

```js
import { useInfomap } from "@mapequation/infomap/react";
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

## Package notes

- `@mapequation/infomap/react` is the supported React entrypoint.

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
