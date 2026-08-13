# JSON test schemas

These schemas validate JSON contracts produced by the C++ runtime and CLI, plus
the internal C++/JS output-format metadata.

Benchmark reports are intentionally excluded because they are Python CI
artifacts, not C++ output contracts.

## Input contracts

`infomap-network.schema.json` is the normative schema for the
`infomap-network` v1.0 **input** format (RFC #645). Unlike the output
schemas, it constrains data the parser reads. This makes it authoritative for
the SAX parser's accept/reject set. The fixtures in
`test/fixtures/networks/json/` (valid) and
`test/fixtures/networks/json/invalid/` (rejected) prove parity. Later phases
reuse the same valid fixtures as parser inputs. Edge weights are deliberately
unbounded (the core ignores `<= 0`; it is not an error); only node/state
weights must be non-negative.
