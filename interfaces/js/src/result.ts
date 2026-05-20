import type { Result } from "./index";
import outputFormatsManifestJson from "../generated/output-formats.json";

export type ResultKey = keyof Result;

export type ResultFormat = {
  key: ResultKey;
  name: string;
  isStates: boolean;
  suffix: string;
  extension: string;
  mimeType: string;
};

export type ResultFile = ResultFormat & {
  filename: string;
  content: string;
};

export type ResultMetadata = {
  codelength: number | null;
  numLevels: number | null;
};

type ResultFormatManifest = {
  resultOrder: ResultKey[];
  formats: {
    optionName: string;
    files: ResultFormat[];
  }[];
};

const outputFormatsManifest = outputFormatsManifestJson as ResultFormatManifest;

const resultFormatsByKey = new Map<ResultKey, ResultFormat>(
  outputFormatsManifest.formats
    .flatMap((format) => format.files)
    .map((format) => [format.key, format]),
);

export const resultFormats: ResultFormat[] =
  outputFormatsManifest.resultOrder.map((key) => {
    const format = resultFormatsByKey.get(key);
    if (!format) {
      throw new Error(`Missing result format metadata for ${key}`);
    }
    return format;
  });

function parseJsonMetadata(result: Result): ResultMetadata {
  const json = result.json_states ?? result.json;
  if (!json) return { codelength: null, numLevels: null };

  const source = json as {
    codelength?: unknown;
    numLevels?: unknown;
  };
  const codelength = Number(source.codelength);
  const numLevels = Number(source.numLevels);

  return {
    codelength: Number.isFinite(codelength) ? codelength : null,
    numLevels: Number.isFinite(numLevels) ? numLevels : null,
  };
}

function textResultContent(result: Result) {
  return resultFormats
    .filter((format) => format.key !== "json" && format.key !== "json_states")
    .map((format) => result[format.key])
    .filter((value): value is string => typeof value === "string")
    .join("\n");
}

function parseTextHeaderMetadata(result: Result): ResultMetadata {
  const content = textResultContent(result);
  const codelengthMatch = content.match(/#\s*codelength\s+([0-9.eE+-]+)/i);
  const numLevelsMatch = content.match(
    /#\s*partitioned into\s+(\d+)\s+levels/i,
  );
  const codelength = codelengthMatch?.[1]
    ? Number(codelengthMatch[1])
    : Number.NaN;
  const numLevels = numLevelsMatch?.[1]
    ? Number(numLevelsMatch[1])
    : Number.NaN;

  return {
    codelength: Number.isFinite(codelength) ? codelength : null,
    numLevels: Number.isFinite(numLevels) ? numLevels : null,
  };
}

function parseTreeLevels(result: Result) {
  const content = [
    result.tree_states,
    result.ftree_states,
    result.tree,
    result.ftree,
  ].join("\n");
  let maxDepth = 0;

  for (const line of content.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (
      !trimmed ||
      trimmed.startsWith("#") ||
      trimmed.startsWith("%") ||
      trimmed.startsWith("*")
    ) {
      continue;
    }

    const [path] = trimmed.split(/\s+/);
    if (!/^\d+(?::\d+)+$/.test(path)) continue;
    maxDepth = Math.max(maxDepth, path.split(":").length);
  }

  return maxDepth > 0 ? maxDepth : null;
}

export function getResultMetadata(result: Result): ResultMetadata {
  const jsonMetadata = parseJsonMetadata(result);
  const textHeaderMetadata = parseTextHeaderMetadata(result);
  return {
    codelength: jsonMetadata.codelength ?? textHeaderMetadata.codelength,
    numLevels:
      jsonMetadata.numLevels ??
      textHeaderMetadata.numLevels ??
      parseTreeLevels(result),
  };
}

export function getResultFiles(result: Result, basename: string): ResultFile[] {
  return resultFormats.flatMap((format) => {
    const value = result[format.key];
    if (value == null) return [];

    const content =
      format.key === "json" || format.key === "json_states"
        ? JSON.stringify(value, null, 2)
        : typeof value === "string"
          ? value
          : JSON.stringify(value, null, 2);

    return [
      {
        ...format,
        filename: `${basename}${format.suffix}.${format.extension}`,
        content,
      },
    ];
  });
}
