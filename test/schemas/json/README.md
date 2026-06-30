# JSON test schemas

These schemas validate JSON contracts produced by the C++ runtime and CLI, plus
the internal C++/JS output-format metadata.

Benchmark reports are intentionally excluded because they are Python CI
artifacts, not C++ output contracts.

## Input contracts

`infomap-network.schema.json` is the normative schema for the
`infomap-network` v1.0 **input** format (RFC #645). Unlike the output
schemas it constrains data the parser reads, so it is authoritative for the SAX
parser's accept/reject set: the fixtures in `test/fixtures/networks/json/`
(valid) and `test/fixtures/networks/json/invalid/` (rejected) prove parity, and
the same valid fixtures are reused as parser inputs in later phases. Edge weights
are deliberately unbounded (`<= 0` is ignored by the core, not an error); only
node/state weights must be non-negative.
