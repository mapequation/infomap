import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, test } from "vitest";

// Drift guard for the hand-written infomap-network-json authoring types.
// The TS types in src/network.ts are maintained by hand (the schema's if/then
// discrimination doesn't survive automatic codegen as a clean union). This test
// fails if the schema gains or drops a property the types don't mirror.

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, "../../../..");

const schema = JSON.parse(
  readFileSync(
    path.join(repoRoot, "test/schemas/json/infomap-network-json.schema.json"),
    "utf8",
  ),
);
const networkSrc = readFileSync(
  path.join(here, "../../src/network.ts"),
  "utf8",
);

/** Property names declared in a TS `export interface <name> { ... }` block. */
function interfaceKeys(name: string): string[] {
  const match = networkSrc.match(
    new RegExp(`export interface ${name}\\s*\\{([\\s\\S]*?)\\n\\}`, "m"),
  );
  if (!match) throw new Error(`interface ${name} not found in network.ts`);
  const keys: string[] = [];
  for (const line of match[1].split("\n")) {
    const key = line.match(/^\s*([A-Za-z_][A-Za-z0-9_]*)\??\s*:/);
    if (key) keys.push(key[1]);
  }
  return keys.sort();
}

/** Real (non-forbidden) property names from a schema `properties` object. */
function schemaKeys(properties: Record<string, unknown>): string[] {
  return Object.entries(properties)
    .filter(([, value]) => value !== false)
    .map(([key]) => key)
    .sort();
}

describe("infomap-network-json authoring types stay in sync with the schema", () => {
  test("root object", () => {
    expect(interfaceKeys("InfomapNetworkJson")).toEqual(
      schemaKeys(schema.properties),
    );
  });

  test("node", () => {
    expect(interfaceKeys("InfomapNetworkJsonNode")).toEqual(
      schemaKeys(schema.$defs.node.properties),
    );
  });

  test("state", () => {
    expect(interfaceKeys("InfomapNetworkJsonState")).toEqual(
      schemaKeys(schema.$defs.state.properties),
    );
  });

  test("edge (union of standard and multilayer edge fields)", () => {
    const edgeFields = new Set([
      ...schemaKeys(schema.$defs.standardEdge.properties),
      ...schemaKeys(schema.$defs.multilayerEdge.properties),
    ]);
    expect(interfaceKeys("InfomapNetworkJsonEdge")).toEqual(
      [...edgeFields].sort(),
    );
  });
});
