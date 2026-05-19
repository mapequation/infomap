import { describe, expect, test } from "vitest";
import networkToString from "../../src/network";

describe("networkToString", () => {
  test("serializes a first-order network with default weights omitted", () => {
    expect(
      networkToString({
        nodes: [{ id: 1, name: "A", weight: 2 }],
        links: [
          { source: 1, target: 2 },
          { source: 2, target: 2, weight: 3 },
        ],
      }),
    ).toBe('*Vertices\n1 "A" 2\n*Links\n1 2\n2 2 3\n');
  });

  test("serializes a simple state network", () => {
    expect(
      networkToString({
        nodes: [{ id: 1, name: "A" }],
        states: [{ stateId: 1, id: 1, name: "A/1" }],
        links: [{ source: 1, target: 1, weight: 2 }],
      }),
    ).toContain("*States");
  });

  test("serializes full multilayer links", () => {
    expect(
      networkToString({
        links: [
          { sourceLayer: 1, source: 2, targetLayer: 3, target: 4 },
          { sourceLayer: 2, source: 4, targetLayer: 1, target: 2, weight: 0.5 },
        ],
      }),
    ).toBe("*Multilayer\n1 2 3 4\n2 4 1 2 0.5\n");
  });

  test("serializes intra and inter multilayer links", () => {
    expect(
      networkToString({
        intra: [{ layerId: 1, source: 2, target: 3 }],
        inter: [{ sourceLayer: 1, id: 2, targetLayer: 3, weight: 0.25 }],
      }),
    ).toBe("*Intra\n1 2 3\n*Inter\n1 2 3 0.25\n");
  });

  test("rejects malformed network objects with explicit errors", () => {
    expect(() => networkToString({} as never)).toThrow(
      "network must contain links, states, or intra links",
    );
    expect(() =>
      networkToString({ links: [{ source: "1", target: 2 }] } as never),
    ).toThrow("network.links[0].source must be a finite number");
    expect(() => networkToString({ states: [], links: [] } as never)).toThrow(
      "network.nodes must be an array",
    );
  });
});
