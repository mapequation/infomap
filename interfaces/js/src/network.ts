export interface Node {
  id: number;
  name?: string;
  weight?: number;
}

export interface StateNode extends Omit<Node, "weight"> {
  stateId: number;
}

export interface Link {
  source: number;
  target: number;
  weight?: number;
}

export interface Network<LinkType = Link> {
  nodes?: Node[];
  links: LinkType[];
}

export interface BipartiteNetwork extends Network {
  bipartiteStartId: number;
}

export interface StateNetwork extends Required<Network> {
  states: StateNode[];
}

export type BipartiteStateNetwork = Required<BipartiteNetwork> & StateNetwork;

export interface MultilayerLink extends Link {
  sourceLayer: number;
  targetLayer: number;
}

export interface IntraLink extends Link {
  layerId: number;
}

export interface InterLink extends Omit<MultilayerLink, "source" | "target"> {
  id: number;
}

export type MultilayerNetwork = Network<MultilayerLink>;

export interface MultilayerIntraInterNetwork extends Omit<Network, "links"> {
  intra: IntraLink[];
  inter?: InterLink[];
}

export type NetworkTypes =
  | Network
  | BipartiteNetwork
  | StateNetwork
  | BipartiteStateNetwork
  | MultilayerNetwork
  | MultilayerIntraInterNetwork;

const isBipartite = (network: NetworkTypes): network is BipartiteNetwork => {
  const net = network as BipartiteNetwork;
  return "bipartiteStartId" in net && !("states" in net);
};

const isState = (network: NetworkTypes): network is StateNetwork => {
  const net = network as StateNetwork;
  return "states" in net && !("bipartiteStartId" in net);
};

const isBipartiteState = (
  network: NetworkTypes,
): network is BipartiteStateNetwork => {
  const net = network as BipartiteStateNetwork;
  return "bipartiteStartId" in net && "states" in net;
};

const isMultilayer = (network: NetworkTypes): network is MultilayerNetwork => {
  const net = network as MultilayerNetwork;
  return (
    "links" in net &&
    Array.isArray(net.links) &&
    net.links.length > 0 &&
    net.links[0] != null &&
    typeof net.links[0] === "object" &&
    "sourceLayer" in net.links[0] &&
    "targetLayer" in net.links[0]
  );
};

const isMultilayerIntraInter = (
  network: NetworkTypes,
): network is MultilayerIntraInterNetwork => {
  const net = network as MultilayerIntraInterNetwork;
  return "intra" in net;
};

export default function stringifyNetwork(network: NetworkTypes) {
  assertRecord(network, "network");

  if (isMultilayerIntraInter(network)) {
    assertMultilayerIntraInterNetwork(network);
    return multilayerIntraInterToString(network);
  } else if (isMultilayer(network)) {
    assertMultilayerNetwork(network);
    return multilayerToString(network);
  } else if (isBipartiteState(network)) {
    assertBipartiteStateNetwork(network);
    return bipartiteStateToString(network);
  } else if (isBipartite(network)) {
    assertBipartiteNetwork(network);
    return bipartiteToString(network);
  } else if (isState(network)) {
    assertStateNetwork(network);
    return stateToString(network);
  } else if ("links" in network) {
    assertNetwork(network);
    return networkToString(network);
  }

  throw new TypeError("network must contain links, states, or intra links");
}

function assertRecord(value: unknown, path: string): asserts value is object {
  if (value == null || typeof value !== "object") {
    throw new TypeError(`${path} must be an object`);
  }
}

function assertArray(value: unknown, path: string): asserts value is unknown[] {
  if (!Array.isArray(value)) {
    throw new TypeError(`${path} must be an array`);
  }
}

function assertNumber(value: unknown, path: string) {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    throw new TypeError(`${path} must be a finite number`);
  }
}

function assertOptionalNumber(value: unknown, path: string) {
  if (value != null) {
    assertNumber(value, path);
  }
}

function assertOptionalString(value: unknown, path: string) {
  if (value != null && typeof value !== "string") {
    throw new TypeError(`${path} must be a string`);
  }
}

function assertNodes(nodes: unknown, path: string, required = false) {
  if (nodes == null) {
    if (required) throw new TypeError(`${path} must be an array`);
    return;
  }

  assertArray(nodes, path);
  nodes.forEach((node, index) => {
    assertRecord(node, `${path}[${index}]`);
    const item = node as Node;
    assertNumber(item.id, `${path}[${index}].id`);
    assertOptionalString(item.name, `${path}[${index}].name`);
    assertOptionalNumber(item.weight, `${path}[${index}].weight`);
  });
}

function assertStates(states: unknown, path: string) {
  assertArray(states, path);
  states.forEach((node, index) => {
    assertRecord(node, `${path}[${index}]`);
    const item = node as StateNode;
    assertNumber(item.stateId, `${path}[${index}].stateId`);
    assertNumber(item.id, `${path}[${index}].id`);
    assertOptionalString(item.name, `${path}[${index}].name`);
  });
}

function assertLinks(links: unknown, path: string) {
  assertArray(links, path);
  links.forEach((link, index) => {
    assertRecord(link, `${path}[${index}]`);
    const item = link as Link;
    assertNumber(item.source, `${path}[${index}].source`);
    assertNumber(item.target, `${path}[${index}].target`);
    assertOptionalNumber(item.weight, `${path}[${index}].weight`);
  });
}

function assertMultilayerLinks(links: unknown, path: string) {
  assertLinks(links, path);
  (links as MultilayerLink[]).forEach((link, index) => {
    assertNumber(link.sourceLayer, `${path}[${index}].sourceLayer`);
    assertNumber(link.targetLayer, `${path}[${index}].targetLayer`);
  });
}

function assertIntraLinks(links: unknown, path: string) {
  assertLinks(links, path);
  (links as IntraLink[]).forEach((link, index) => {
    assertNumber(link.layerId, `${path}[${index}].layerId`);
  });
}

function assertInterLinks(links: unknown, path: string) {
  if (links == null) return;

  assertArray(links, path);
  links.forEach((link, index) => {
    assertRecord(link, `${path}[${index}]`);
    const item = link as InterLink;
    assertNumber(item.sourceLayer, `${path}[${index}].sourceLayer`);
    assertNumber(item.id, `${path}[${index}].id`);
    assertNumber(item.targetLayer, `${path}[${index}].targetLayer`);
    assertOptionalNumber(item.weight, `${path}[${index}].weight`);
  });
}

function assertNetwork(network: Network) {
  assertNodes(network.nodes, "network.nodes");
  assertLinks(network.links, "network.links");
}

function assertMultilayerNetwork(network: MultilayerNetwork) {
  assertNodes(network.nodes, "network.nodes");
  assertMultilayerLinks(network.links, "network.links");
}

function assertBipartiteNetwork(network: BipartiteNetwork) {
  assertNodes(network.nodes, "network.nodes");
  assertLinks(network.links, "network.links");
  assertNumber(network.bipartiteStartId, "network.bipartiteStartId");
}

function assertStateNetwork(network: StateNetwork) {
  assertNodes(network.nodes, "network.nodes", true);
  assertStates(network.states, "network.states");
  assertLinks(network.links, "network.links");
}

function assertBipartiteStateNetwork(network: BipartiteStateNetwork) {
  assertStateNetwork(network);
  assertNumber(network.bipartiteStartId, "network.bipartiteStartId");
}

function assertMultilayerIntraInterNetwork(
  network: MultilayerIntraInterNetwork,
) {
  assertNodes(network.nodes, "network.nodes");
  assertIntraLinks(network.intra, "network.intra");
  assertInterLinks(network.inter, "network.inter");
}

function multilayerIntraInterToString(network: MultilayerIntraInterNetwork) {
  let result = "";

  if (network.nodes != null) {
    result += nodesToString(network.nodes);
  }

  if (network.intra.length > 0) {
    result += "*Intra\n";

    for (const { layerId, source, target, weight } of network.intra) {
      result += `${layerId} ${source} ${target}`;
      if (weight != null) result += ` ${weight}`;
      result += "\n";
    }
  }

  if (network.inter != null && network.inter.length > 0) {
    result += "*Inter\n";

    for (const { sourceLayer, id, targetLayer, weight } of network.inter) {
      result += `${sourceLayer} ${id} ${targetLayer}`;
      if (weight != null) result += ` ${weight}`;
      result += "\n";
    }
  }

  return result;
}

function multilayerToString(network: MultilayerNetwork) {
  let result = "";

  if (network.nodes != null) {
    result += nodesToString(network.nodes);
  }

  if (network.links.length > 0) {
    result += "*Multilayer\n";

    for (const {
      sourceLayer,
      source,
      targetLayer,
      target,
      weight,
    } of network.links) {
      result += `${sourceLayer} ${source} ${targetLayer} ${target}`;
      if (weight != null) result += ` ${weight}`;
      result += "\n";
    }
  }

  return result;
}

function bipartiteStateToString(network: BipartiteStateNetwork) {
  let result = "";

  result += nodesToString(network.nodes);

  result += statesToString(network.states);

  result += linksToString(
    network.links,
    `*Bipartite ${network.bipartiteStartId}\n`,
  );

  return result;
}

function bipartiteToString(network: BipartiteNetwork) {
  let result = "";

  if (network.nodes != null) {
    result += nodesToString(network.nodes);
  }

  result += linksToString(
    network.links,
    `*Bipartite ${network.bipartiteStartId}\n`,
  );

  return result;
}

function stateToString(network: StateNetwork) {
  let result = "";

  result += nodesToString(network.nodes);

  result += statesToString(network.states);

  result += linksToString(network.links);

  return result;
}

function networkToString(network: Network) {
  let result = "";

  if (network.nodes != null) {
    result += nodesToString(network.nodes);
  }

  result += linksToString(network.links);

  return result;
}

function nodesToString(nodes: Node[]) {
  if (nodes.length === 0) {
    return "";
  }

  let result = "*Vertices\n";

  for (const { id, name, weight } of nodes) {
    result += id;
    if (name != null) result += ` "${name}"`;
    else result += ` "${id}"`;
    if (weight != null) result += ` ${weight}`;
    result += "\n";
  }

  return result;
}

function statesToString(nodes: StateNode[]) {
  if (nodes.length === 0) {
    return "";
  }

  let result = "*States\n";

  for (const { stateId, id, name } of nodes) {
    result += `${stateId} ${id}`;
    if (name != null) result += ` "${name}"`;
    result += "\n";
  }

  return result;
}

function linksToString(links: Link[], header = "*Links\n") {
  if (links.length === 0) {
    return "";
  }

  let result = header;

  for (const { source, target, weight } of links) {
    result += `${source} ${target}`;
    if (weight != null) result += ` ${weight}`;
    result += "\n";
  }

  return result;
}
