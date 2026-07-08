# Nanobind spike (#743)

A minimal nanobind backend behind the `Core` boundary, built to answer the
go/no-go question in issue #743. **Not wired into packaging** — the shipped
extension is still SWIG.

## What it covers

- `Core`-subset binding of `InfomapWrapper` (construct-from-flags, build
  verbs, `run`, `getModules`, scalars, one writer)
- Zero-copy NumPy views over `NodeData` (`get_node_data` returns a dict of
  ndarrays whose owner capsule shares one `shared_ptr<NodeData>`; today's
  SWIG path copies every column into a Python list)
- C++ exceptions raised directly as the public taxonomy
  (`infomap.errors.InfomapError` / `NetworkParseError`) at the binding layer
- Coexistence with the SWIG extension in one process (requires the symbol
  hygiene in `CMakeLists.txt`: `-fvisibility=hidden` +
  `--exclude-libs ALL` — both extensions embed the engine)

## What it deliberately skips (the bulk of a real port)

The live tree-walking iterator surface (`InfoNode`, the five iterator types
— public, documented API), the `network()` sub-object, most writers, meta
data, the multilayer bulk paths, and the build-backend migration
(setuptools + pregenerated SWIG C++ → CMake/scikit-build-core).

## Build and run

```sh
.venv/bin/pip install nanobind
cmake -S spikes/nanobind -B build/nanobind-spike \
  -DCMAKE_BUILD_TYPE=Release \
  -DPython_EXECUTABLE=$PWD/.venv/bin/python \
  -Dnanobind_DIR=$(.venv/bin/python -m nanobind --cmake_dir)
cmake --build build/nanobind-spike -j
.venv/bin/python spikes/nanobind/bench.py
```

## Results (Linux x86-64, Python 3.11, GCC, -O3, OpenMP)

|  | SWIG (shipped) | nanobind spike |
|---|---|---|
| Extension size | 3.9 MB (3.4 MB stripped) + 111 kB shadow module | 0.9 MB |
| Import time (best of 5, fresh process) | 40.6 ms | 1.7 ms |
| `get_node_data`, 100k nodes / 500k links | 325 ms (today's list path); 383 ms via `np.asarray` | **17 ms, zero-copy** |
| Result parity (modules, codelength, all NodeData columns) | — | identical, given the same `addLink` sequence |

See the full go/no-go report on issue #743.
