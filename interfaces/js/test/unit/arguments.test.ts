import { describe, expect, test } from "vitest";
import argumentsToString from "../../src/arguments";

describe("argumentsToString", () => {
  test("serializes booleans and scalar values", () => {
    expect(
      argumentsToString({
        twoLevel: true,
        numTrials: 5,
        output: ["tree", "clu"],
        pretty: true,
        help: "advanced",
      }),
    ).toBe(" --output tree,clu --pretty --two-level --num-trials 5 -hh");
  });

  test("serializes variable Markov minimum scale", () => {
    expect(
      argumentsToString({
        variableMarkovMinScale: 0.5,
      }),
    ).toBe(" --variable-markov-min-scale 0.5");
  });

  test("serializes regularized multilayer options", () => {
    expect(
      argumentsToString({
        hardPartition: true,
        multilayerSelfInterLinks: true,
        randomNodeCheckRate: 0.25,
        multilayerTest: 2,
        multilayerAggregation: true,
      }),
    ).toBe(
      " --multilayer-self-inter-links --hard-partition --random-node-check-rate 0.25 --multilayer-test 2 --multilayer-aggregation",
    );
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
});
