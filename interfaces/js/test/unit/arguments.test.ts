import { describe, expect, test } from "vitest";
import argumentsToString from "../../src/arguments";

describe("argumentsToString", () => {
  test("serializes booleans and scalar values", () => {
    expect(
      argumentsToString({
        twoLevel: true,
        numTrials: 5,
        output: ["tree", "clu"],
        help: "advanced",
      }),
    ).toBe(" --output tree,clu --two-level --num-trials 5 -hh");
  });

  test("serializes variable Markov minimum scale", () => {
    expect(
      argumentsToString({
        variableMarkovMinScale: 0.5,
      }),
    ).toBe(" --variable-markov-min-scale 0.5");
  });

  test("serializes incremental and binding-only options", () => {
    expect(
      argumentsToString({
        verbose: 3,
        fastHierarchicalSolution: 2,
        version: true,
      }),
    ).toBe(" -vvv -FF --version");
  });

  // The rendered string is split on whitespace by the engine, with no quoting, so
  // a whitespace-bearing value does not stay one token. Through this worker that
  // is worse than a truncation: it injects options and silently changes the
  // algorithm rather than failing.
  describe("rejects values the argument string cannot carry", () => {
    test.each([
      ["outName", { outName: "my run" }],
      ["clusterData", { clusterData: "a b.clu" }],
      ["metaData", { metaData: "meta data.txt" }],
      ["summaryJson", { summaryJson: "out dir/summary.json" }],
      ["timingJson", { timingJson: "out dir/timing.json" }],
      ["manifestJson", { manifestJson: "out dir/manifest.json" }],
    ])("%s containing whitespace", (name, args) => {
      expect(() => argumentsToString(args)).toThrow(
        new RegExp(`${name}=.*contains whitespace`),
      );
    });

    test("an option-injecting value is refused, not rendered", () => {
      expect(() =>
        argumentsToString({ outName: "net --directed -o tree" }),
      ).toThrow(/contains whitespace/);
    });

    // A choice-valued option's string-literal union is a compile-time constraint
    // only; this renderer is reachable from plain JavaScript, so the guard has to
    // hold at runtime too. Cast through the type to reach that path.
    test("a choice-valued option is guarded at runtime, not just by its type", () => {
      expect(() =>
        argumentsToString({
          flowModel: "undirected --two-level" as never,
        }),
      ).toThrow(/flowModel=.*contains whitespace/);
    });

    test("the comma-list output option is guarded in both its forms", () => {
      expect(() =>
        argumentsToString({ output: "tree -N 99" as never }),
      ).toThrow(/output=.*contains whitespace/);
      expect(() =>
        argumentsToString({ output: ["tree", "clu -N 99"] as never }),
      ).toThrow(/output=.*contains whitespace/);
    });

    test("whitespace-free values of the same options still render", () => {
      expect(
        argumentsToString({ outName: "my-run", clusterData: "seed.clu" }),
      ).toBe(" --cluster-data seed.clu --out-name my-run");
    });
  });
});
