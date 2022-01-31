import type {
  NodeBase,
  CluNode,
  CluStateNode,
  TreeNode as JsonTreeNode,
  TreeStateNode as JsonTreeStateNode,
} from "./filetypes";
import type { Header as JsonHeader, Module } from "./index";

type Optional<T, K extends keyof T> = Pick<Partial<T>, K> & Omit<T, K>;

type Header = Optional<JsonHeader, "directed">;
type TreeNode = Omit<JsonTreeNode, "modules" | "modularCentrality">;
type TreeStateNode = Omit<JsonTreeStateNode, "modules" | "modularCentrality">;

export type Result<NodeType extends NodeBase> = Header & {
  nodes: NodeType[];
  modules?: Module[];
};

export type Extension = "clu" | "tree";

export function extension(filename: string) {
  return filename.split(".").pop() ?? null;
}

export function readFile(file: File) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = reject;
    reader.readAsText(file);
  });
}

export function parse(file: string | string[], type?: Extension, parseLinks = false) {
  if (typeof file === "string") {
    file = file.split("\n");
  }

  const nodeHeader = parseNodeHeader(file);

  if (nodeHeader === null) {
    return null;
  }

  if ((type && type === "clu") || nodeHeader.includes("module")) {
    if (nodeHeader[0] === "node_id") {
      return parseClu(file, nodeHeader);
    } else if (nodeHeader[0] === "state_id") {
      return parseClu<CluStateNode>(file, nodeHeader);
    }
  } else if ((type && type === "tree") || nodeHeader.includes("path")) {
    if (nodeHeader[nodeHeader.length - 2] === "name") {
      return parseTree(file, nodeHeader, parseLinks);
    } else if (nodeHeader[nodeHeader.length - 2] === "state_id" || nodeHeader[nodeHeader.length - 1] === "layer_id") {
      return parseTree<TreeStateNode>(file, nodeHeader, parseLinks);
    }
  }

  return null;
}

const map = {
  node_id: "id",
  module: "moduleId",
  flow: "flow",
  name: "name",
  state_id: "stateId",
  layer_id: "layerId",
  path: "path",
} as const;

export function parseClu<NodeType extends CluNode>(
  file: string | string[],
  nodeHeader?: string[]
): Result<NodeType> | null {
  // First order
  // # node_id module flow
  // 10 1 0.0384615
  // 11 1 0.0384615
  // 12 1 0.0384615

  // States
  // # state_id module flow node_id
  // 1 1 0.166667 1
  // 2 1 0.166667 2
  // 3 1 0.166667 3

  // Multilayer
  // # state_id module flow node_id layer_id
  // 3 1 0.166667 1 2
  // 4 1 0.166667 2 2
  // 5 1 0.166667 3 2

  if (typeof file === "string") {
    file = file.split("\n");
  }

  const header = parseHeader(file);
  if (!header) {
    console.warn("No header");
    return null;
  }

  if (!nodeHeader) {
    nodeHeader = parseNodeHeader(file) ?? [];
    if (nodeHeader.length === 0) {
      console.warn("No node header");
      return null;
    }
  }

  const nodes: NodeType[] = [];

  for (const [_, line] of nodeSection(file)) {
    const fields = line.split(" ").map(Number);

    if (fields.length < nodeHeader.length) {
      continue;
    }

    const node: Partial<NodeType> = {};

    for (let i = 0; i < nodeHeader.length; ++i) {
      const field = nodeHeader[i] as keyof typeof map;
      const key = map[field] as keyof NodeType;
      // @ts-ignore
      node[key] = fields[i];
    }

    nodes.push(node as NodeType);
  }

  return {
    ...header,
    nodes,
  };
}

export function parseTree<NodeType extends TreeNode>(
  file: string | string[],
  nodeHeader?: string[],
  parseLinks = false
): Result<NodeType> | null {
  // First order
  // # path flow name node_id
  // 1:1 0.166667 "i" 1
  // 1:2 0.166667 "j" 2
  // 1:3 0.166667 "k" 3

  // States
  // # path flow name state_id node_id
  // 1:1 0.166667 "i" 1 1
  // 1:2 0.166667 "j" 2 2
  // 1:3 0.166667 "k" 3 3

  // Multilayer
  // # path flow name state_id node_id layer_id
  // 1:1 0.166667 "i" 3 1 2
  // 1:2 0.166667 "j" 4 2 2
  // 1:3 0.166667 "k" 5 3 2

  if (typeof file === "string") {
    file = file.split("\n");
  }

  const header = parseHeader(file);
  if (!header) {
    console.warn("No header");
    return null;
  }

  if (!nodeHeader) {
    nodeHeader = parseNodeHeader(file) ?? [];
    if (nodeHeader.length === 0) {
      console.warn("No node header");
      return null;
    }
  }

  const nodes: NodeType[] = [];

  let lineNo = 0;

  for (const [i, line] of nodeSection(file)) {
    lineNo = i;

    const match = line.match(/[^\s"']+|"([^"]*)"/g);

    if (match === null || match.length < nodeHeader.length) {
      continue;
    }

    const node: Partial<NodeType> = {};

    for (let j = 0; j < nodeHeader.length; ++j) {
      const field = nodeHeader[j] as keyof typeof map;
      const key = map[field] as keyof NodeType;

      switch (field) {
        case "path":
          node.path = match[j].split(":").map(Number);
          break;
        case "name":
          node.name = match[j].slice(1, -1);
          break;
        case "node_id":
          node.id = Number(match[j]);
          break;
        case "state_id":
        case "layer_id":
        case "flow":
          // @ts-ignore
          node[key] = Number(match[j]);
          break;
        default:
          console.warn(`Unknown field ${field}`);
          break;
      }
    }

    nodes.push(node as NodeType);
  }

  // lineNo is the index of the last line of the node section
  ++lineNo;

  if (file.length <= lineNo + 1) {
    return {
      ...header,
      nodes,
    };
  }

  const modules = [];
  let module: Module | null = null;

  const linkHeader = file[lineNo];

  let directed = false;

  if (linkHeader?.startsWith("*Links")) {
    const parts = linkHeader.trim().split(" ");
    directed = parts.length > 1 && parts[1] === "directed";
  }

  for (const [_, line] of linkSection(file, lineNo)) {
    if (line.startsWith("*Links")) {
      const [_, moduleId, ...rest] = line.split(" ");

      const path = moduleId === "root" ? [0] : moduleId.split(":").map(Number);
      const [enterFlow, exitFlow, numEdges, numChildren] = rest.map(Number);

      module = {
        path,
        enterFlow,
        exitFlow,
        numEdges,
        numChildren,
        links: parseLinks ? [] : undefined,
      };

      modules.push(module);
      continue;
    }

    if (!parseLinks || !module) {
      continue;
    }

    const [source, target, flow] = line.split(" ").map(Number);

    module.links?.push({ source, target, flow });
  }

  return {
    ...header,
    directed,
    nodes,
    modules,
  };
}

function* nodeSection(lines: string[]) {
  for (let i = 0; i < lines.length; ++i) {
    const line = lines[i];

    if (line.startsWith("#") || line.length === 0) {
      continue;
    }

    if (line.startsWith("*")) {
      break;
    }

    yield [i, line] as [number, string];
  }
}

function* linkSection(lines: string[], start: number = 0) {
  // links section start at lines[start]
  if (lines.length <= start || !lines[start].startsWith("*Links")) {
    return;
  }

  // skip the section header
  start++;

  for (let i = start; i < lines.length; ++i) {
    const line = lines[i];

    if (line.length === 0) {
      continue;
    }

    yield [i, line] as [number, string];
  }
}

function parseNodeHeader(file: string | string[]): string[] | null {
  // First order tree
  // # path flow name node_id

  // States tree
  // # path flow name state_id node_id

  // Multilayer tree states
  // # path flow name state_id node_id layer_id

  // First order clu
  // # node_id module flow

  // States clu
  // # state_id module flow node_id

  // Multilayer clu states
  // # state_id module flow node_id layer_id

  const prefixes = ["# path", "# node_id", "# state_id"];

  if (typeof file === "string") {
    file = file.split("\n");
  }

  for (const line of file) {
    if (!line.startsWith("#")) {
      break;
    }

    if (prefixes.some((prefix) => line.startsWith(prefix))) {
      return line.slice(2).split(" ");
    }
  }

  return null;
}

export function parseHeader(file: string | string[]): Header | null {
  // # v1.7.3
  // # ./Infomap examples/networks/ninetriangles.net .
  // # started at 2021-11-02 13:56:33
  // # completed in 0.011717 s
  // # partitioned into 3 levels with 7 top modules
  // # codelength 3.49842 bits
  // # relative codelength savings 26.2781%
  // # bipartite start id 123
  // # path flow name node_id

  const result: Partial<Header> = {};

  if (typeof file === "string") {
    file = file.split("\n");
  }

  if (file.length < 8) return null;

  const version = file[0].match(/^# (v\d+\.\d+\.\d+)/)?.[1];

  if (version) {
    result.version = version;
  } else {
    return null;
  }

  const args = file[1].match(/^# (.+)/)?.[1];

  if (args) {
    result.args = args;
  } else {
    return null;
  }

  for (let i = 2; i < file.length; ++i) {
    const line = file[i];

    if (!line.startsWith("#")) {
      break;
    }

    const startedAt = line.match(/^# started at (.+)/)?.[1];
    if (startedAt) {
      result.startedAt = startedAt;
      continue;
    }

    const completedIn = line.match(/^# completed in (.+) s/)?.[1];
    if (completedIn) {
      result.completedIn = Number(completedIn);
      continue;
    }

    const partitionedInto = line.match(
      /^# partitioned into (\d+) levels with (\d+) top modules/
    );
    if (partitionedInto) {
      result.numLevels = Number(partitionedInto[1]);
      result.numTopModules = Number(partitionedInto[2]);
      continue;
    }

    const codelength = line.match(/^# codelength (.+) bits/)?.[1];
    if (codelength) {
      result.codelength = Number(codelength);
      continue;
    }

    const relativeCodelengthSavings = line.match(
      /^# relative codelength savings (.+)%/
    )?.[1];
    if (relativeCodelengthSavings) {
      result.relativeCodelengthSavings =
        Number(relativeCodelengthSavings) / 100;
      continue;
    }

    const bipartiteStartId = line.match(/^# bipartite start id (\d+)/)?.[1];
    if (bipartiteStartId) {
      result.bipartiteStartId = Number(bipartiteStartId);
      //continue;
    }
  }

  return result as Header;
}
