// Run the differential corpus (cases.tsv) through the Node API of the built npm
// package (make build-js).
//
//   node test/differential/run_js.mjs <out-dir>
//
// Run from the repository root. Writes the layout differential.py compares:
// <id>.tree and <id>.codelength per case, <id>.error, or <id>.skip.
import fs from "node:fs";
import path from "node:path";
import { run } from "../../dist/npm/package/node.js";

const [outDir] = process.argv.slice(2);
if (!outDir) {
  throw new Error("usage: run_js.mjs <out-dir>");
}
fs.mkdirSync(outDir, { recursive: true });

// run() takes the network as a string and has no way to pass other input
// files, so options that name a file cannot be run here.
const FILE_OPTIONS = new Set(["meta_data", "cluster_data"]);

const lines = fs
  .readFileSync("test/differential/cases.tsv", "utf8")
  .split("\n")
  .filter((line) => line && !line.startsWith("#"));
if (lines[0] !== "id\tnetwork\toptions") {
  throw new Error(`unexpected header ${lines[0]}`);
}

for (const line of lines.slice(1)) {
  const [id, network, options = ""] = line.split("\t");
  const tokens = options.split(" ").filter(Boolean);
  const write = (suffix, text) =>
    fs.writeFileSync(path.join(outDir, `${id}.${suffix}`), `${text}\n`);

  const fileOption = tokens.find((token) =>
    FILE_OPTIONS.has(token.split("=")[0]),
  );
  if (fileOption) {
    write("skip", `run() takes no input file for ${fileOption}`);
    continue;
  }

  const args = ["--silent", "--output", "tree,json"];
  if (!tokens.some((token) => token.startsWith("seed="))) {
    args.push("--seed", "123");
  }
  for (const token of tokens) {
    const [name, value] = token.split("=");
    args.push(`--${name.replaceAll("_", "-")}`);
    if (value !== undefined) {
      args.push(value);
    }
  }

  try {
    const result = await run(fs.readFileSync(network, "utf8"), {
      filename: path.basename(network),
      args,
    });
    write("tree", result.tree_states ?? result.tree);
    write("codelength", String(result.json.codelength));
  } catch (error) {
    write("error", error.message);
  }
}
