import { describe, expect, test } from "vitest";
import networkToString from "../../src/network";

describe("networkToString", () => {
  test("serializes a simple state network", () => {
    expect(
      networkToString({
        nodes: [{ id: 1, name: "A" }],
        states: [{ stateId: 1, id: 1, name: "A/1" }],
        links: [{ source: 1, target: 1, weight: 2 }]
      })
    ).toContain("*States");
  });
});
