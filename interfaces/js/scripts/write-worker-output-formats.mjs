import fs from "node:fs";
import path from "node:path";
import outputFormatsManifest from "../generated/output-formats.json" with {
  type: "json",
};

const [, , outputPath] = process.argv;

if (!outputPath) {
  throw new Error("Usage: write-worker-output-formats.mjs OUTPUT_PATH");
}

const outputFilesByKey = new Map(
  outputFormatsManifest.formats
    .flatMap((format) => format.files)
    .map((file) => [file.key, file]),
);

const resultFiles = outputFormatsManifest.resultOrder.map((key) => {
  const file = outputFilesByKey.get(key);
  if (!file) {
    throw new Error(`Missing worker output format metadata for ${key}`);
  }

  return {
    key: file.key,
    suffix: file.suffix,
    extension: file.extension,
  };
});

fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(
  outputPath,
  `const resultFiles = ${JSON.stringify(resultFiles, null, 2)};\n`,
);
