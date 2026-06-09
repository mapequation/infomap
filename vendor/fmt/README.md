# {fmt} (vendored, header-only subset)

Vendored subset of [{fmt}](https://github.com/fmtlib/fmt) **10.2.1**.

Only the header-only formatting core is bundled:

- `include/fmt/core.h`
- `include/fmt/format.h`
- `include/fmt/format-inl.h`

Deliberately **excluded**: `fmt/ostream.h`, `fmt/os.h`, `fmt/printf.h`,
`fmt/std.h`, `fmt/color.h`. These pull in `<iostream>` / `FILE*` / global
stdio streams, which the R package forbids (see
`scripts/check_cpp_stream_policy.py`). The bundled subset is verified
iostream-free.

## Usage

Do **not** include `<fmt/format.h>` directly. Include the wrapper
[`src/utils/format.h`](../../src/utils/format.h), which defines
`FMT_HEADER_ONLY` consistently across every build surface (CLI, Python, R,
JS/Emscripten). Use `fmt::format(...)` to build strings; never `fmt::print`
(it writes to `stdout`, bypassing the `infomap::Log` sink).

## Updating

Re-fetch the three headers and `LICENSE` from the pinned tag:

```sh
TAG=10.2.1
base="https://raw.githubusercontent.com/fmtlib/fmt/$TAG/include/fmt"
for f in core.h format.h format-inl.h; do curl -fsSL "$base/$f" -o include/fmt/$f; done
curl -fsSL "https://raw.githubusercontent.com/fmtlib/fmt/$TAG/LICENSE" -o LICENSE
```
