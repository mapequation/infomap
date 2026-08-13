// Programmatic Node.js entry point, published as `@mapequation/infomap/node`.
//
// This mirrors how the Python package ships a programmatic module alongside a
// bundled binary: `run()` here is the module, the `infomap` bin (cli.ts) is the
// binary. Both are backed by Emscripten builds of the same C++ engine.
//
//   import { run } from "@mapequation/infomap/node";
//   const result = await run(network, { args: ["-o", "tree,json", "-2"] });
//   console.log(result.json?.codelength, result.tree);
//
// The library artifact uses an in-memory filesystem, so networks never touch
// disk and Infomap's output is captured rather than printed. The result is keyed
// exactly like the browser worker package (clu, tree, ftree, json, ...) and
// contains only the formats Infomap produced.
import outputFormatsManifest from "../generated/output-formats.json";
import type { Result } from "./index";

export type { Result };
export type { Tree, StateTree, Module, Header } from "./index";

export interface RunOptions {
  /** Virtual input filename; its basename also names the outputs. */
  filename?: string;
  /** Extra Infomap CLI options, as a string or pre-split array. */
  args?: string | string[];
  /** Override the output basename (passed as --out-name). */
  outName?: string;
}

const formatByKey = new Map(
  outputFormatsManifest.formats
    .flatMap((format) => format.files)
    .map((file) => [file.key, file] as const),
);

const resultFiles = outputFormatsManifest.resultOrder.map((key) => {
  const format = formatByKey.get(key);
  if (!format) {
    throw new Error(`Missing output format metadata for ${key}`);
  }
  return { key, suffix: format.suffix, extension: format.extension };
});

function basenameWithoutExtension(filename: string) {
  return filename.replace(/\.[^./\\]+$/, "");
}

function exitStatusCode(error: unknown) {
  if (error && typeof error === "object" && "status" in error) {
    return Number((error as { status: unknown }).status) || 0;
  }
  return null;
}

/**
 * Run Infomap on an in-memory network string and return the output files.
 *
 * Infomap's progress is captured (not printed); on a non-zero exit its stderr is
 * included in the thrown error. Pass `--silent` in `args` to quiet the engine.
 */
export async function run(
  network: string,
  options: RunOptions = {},
): Promise<Result> {
  if (typeof network !== "string") {
    throw new TypeError("run(network, options): network must be a string");
  }

  const { filename = "network.net", args = [], outName } = options;
  const extraArgs =
    typeof args === "string" ? args.split(/\s+/).filter(Boolean) : [...args];
  // --out-name inside args is honoured too, the way the worker entry point reads it, so
  // the two surfaces agree. Passing it there used to be silently ignored here: the
  // engine wrote one basename while the results were read back under another.
  // lastIndexOf, mirroring the engine: it takes the last occurrence of an option.
  const outNameFlagIndex = extraArgs.lastIndexOf("--out-name");
  const outNameFromArgs =
    outNameFlagIndex >= 0 ? extraArgs[outNameFlagIndex + 1] : undefined;
  const base = outName ?? outNameFromArgs ?? basenameWithoutExtension(filename);
  if (outName) {
    extraArgs.push("--out-name", outName);
  }

  const { default: createInfomapModule } = await import("./infomap.node.mjs");

  const stdout: string[] = [];
  const stderr: string[] = [];
  const Module = await createInfomapModule({
    noInitialRun: true,
    print: (line) => {
      stdout.push(line);
    },
    printErr: (line) => {
      stderr.push(line);
    },
  });

  const workDir = "/work";
  Module.FS.mkdir(workDir);
  // The input goes in its own directory so it cannot be read back as output: the out
  // name is the input filename minus its extension, so ${workDir}/${base}.net was the
  // input file and the "net" result key handed the caller its own network back (#903).
  const inputDir = `${workDir}/input`;
  Module.FS.mkdir(inputDir);
  const inputPath = `${inputDir}/${filename}`;
  Module.FS.writeFile(inputPath, network);

  let status = 0;
  try {
    const returned = Module.callMain([inputPath, workDir, ...extraArgs]);
    status = typeof returned === "number" ? returned : 0;
  } catch (error) {
    const code = exitStatusCode(error);
    if (code === null) {
      throw error;
    }
    status = code;
  }

  if (status !== 0) {
    throw new Error(
      `Infomap exited with status ${status}\n${stderr.join("\n")}`,
    );
  }

  const result: Result = {};
  let found = 0;
  for (const file of resultFiles) {
    const outputPath = `${workDir}/${base}${file.suffix}.${file.extension}`;
    let content: string;
    try {
      content = Module.FS.readFile(outputPath, { encoding: "utf8" });
    } catch {
      continue;
    }
    found++;
    (result as Record<string, unknown>)[file.key] =
      file.extension === "json" ? JSON.parse(content) : content;
  }

  const expectsOutputFiles = !extraArgs.includes("--no-file-output");
  if (found === 0 && expectsOutputFiles) {
    throw new Error(
      `Infomap exited 0 but none of its output files were found under "${base}". ` +
        "This usually means --out-name and the name the results are read back under " +
        "disagree. Pass --no-file-output if no output files are wanted.",
    );
  }

  return result;
}

export default run;
