import { describe, expect, test } from "vitest";
import type { StateTree, Tree } from "../../src/index";

/*
 * Type-level regression tests, checked by `npm run typecheck`, which covers test/unit.
 * The expect-error directives below are the assertions: if Tree goes back to requiring
 * every node field they become unused, and tsc fails on an unused directive -- so these
 * cannot rot into no-ops.
 *
 * Measured against 2.15.0 output rather than assumed:
 *   states.net        -o json  ->  flow id mec name path            (no modules)
 *   states.net  states json    ->  flow id mec modules name path stateId  (no layerId)
 *   ninetriangles.net -o json  ->  flow id mec modules name path
 */
describe("Tree node fields the JSON output may omit", () => {
  test("modules is optional: a higher-order network's physical JSON has none", () => {
    // Exactly what states.net -o json emits: no modules key.
    const tree = {
      nodes: [{ path: [1, 1], id: 1, flow: 0.25, mec: 0, name: "a" }],
    } as unknown as Tree;
    const node = tree.nodes[0];

    // @ts-expect-error modules is possibly undefined and has to be checked first
    const unchecked: number[] = node.modules;
    const checked: number[] = node.modules ?? [];

    expect(unchecked).toBe(undefined);
    expect(checked).toEqual([]);
  });

  test("layerId is optional: only multilayer input carries it", () => {
    // Exactly what the states JSON emits for non-multilayer input: no layerId key.
    const tree = {
      nodes: [
        {
          path: [1, 1],
          id: 1,
          flow: 0.25,
          mec: 0,
          name: "a",
          stateId: 7,
          modules: [1],
        },
      ],
    } as unknown as StateTree;
    const node = tree.nodes[0];

    // @ts-expect-error layerId is possibly undefined
    const unchecked: number = node.layerId;
    const checked: number = node.layerId ?? 0;

    expect(unchecked).toBe(undefined);
    expect(checked).toBe(0);
  });

  test("stateId stays required, because the states JSON always carries it", () => {
    const tree = {
      nodes: [{ path: [1, 1], id: 1, flow: 0.25, mec: 0, stateId: 7 }],
    } as unknown as StateTree;
    const node = tree.nodes[0];

    const stateId: number = node.stateId;

    expect(stateId).toBe(7);
  });
});
