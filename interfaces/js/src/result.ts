import type { Result } from "./index";

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

const textMimeType = "text/plain;charset=utf-8";
const jsonMimeType = "application/json;charset=utf-8";

export const resultFormats: ResultFormat[] = [
  {
    key: "clu",
    name: "Clu",
    isStates: false,
    suffix: "",
    extension: "clu",
    mimeType: textMimeType,
  },
  {
    key: "tree",
    name: "Tree",
    isStates: false,
    suffix: "",
    extension: "tree",
    mimeType: textMimeType,
  },
  {
    key: "ftree",
    name: "Ftree",
    isStates: false,
    suffix: "",
    extension: "ftree",
    mimeType: textMimeType,
  },
  {
    key: "net",
    name: "Network",
    isStates: false,
    suffix: "",
    extension: "net",
    mimeType: textMimeType,
  },
  {
    key: "states_as_physical",
    name: "States as physical",
    isStates: false,
    suffix: "_states_as_physical",
    extension: "net",
    mimeType: textMimeType,
  },
  {
    key: "clu_states",
    name: "Clu",
    isStates: true,
    suffix: "_states",
    extension: "clu",
    mimeType: textMimeType,
  },
  {
    key: "tree_states",
    name: "Tree",
    isStates: true,
    suffix: "_states",
    extension: "tree",
    mimeType: textMimeType,
  },
  {
    key: "ftree_states",
    name: "Ftree",
    isStates: true,
    suffix: "_states",
    extension: "ftree",
    mimeType: textMimeType,
  },
  {
    key: "states",
    name: "States",
    isStates: true,
    suffix: "_states",
    extension: "net",
    mimeType: textMimeType,
  },
  {
    key: "flow",
    name: "Flow",
    isStates: false,
    suffix: "_flow",
    extension: "net",
    mimeType: textMimeType,
  },
  {
    key: "flow_as_physical",
    name: "Flow states as physical",
    isStates: true,
    suffix: "_states_as_physical_flow",
    extension: "net",
    mimeType: textMimeType,
  },
  {
    key: "newick",
    name: "Newick",
    isStates: false,
    suffix: "",
    extension: "nwk",
    mimeType: textMimeType,
  },
  {
    key: "newick_states",
    name: "Newick",
    isStates: true,
    suffix: "_states",
    extension: "nwk",
    mimeType: textMimeType,
  },
  {
    key: "json",
    name: "JSON",
    isStates: false,
    suffix: "",
    extension: "json",
    mimeType: jsonMimeType,
  },
  {
    key: "json_states",
    name: "JSON",
    isStates: true,
    suffix: "_states",
    extension: "json",
    mimeType: jsonMimeType,
  },
  {
    key: "csv",
    name: "CSV",
    isStates: false,
    suffix: "",
    extension: "csv",
    mimeType: textMimeType,
  },
  {
    key: "csv_states",
    name: "CSV",
    isStates: true,
    suffix: "_states",
    extension: "csv",
    mimeType: textMimeType,
  },
];

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
