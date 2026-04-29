import { describe, expect, test } from "vitest";
import argumentsToString from "../../src/arguments";

describe("argumentsToString", () => {
  test("serializes booleans and scalar values", () => {
    expect(
      argumentsToString({
        twoLevel: true,
        numTrials: 5,
        output: ["tree", "clu"],
        help: "advanced"
      })
    ).toBe(" -o tree,clu --two-level --num-trials 5 -hh");
  });

  test("serializes regularized multilayer options", () => {
    expect(
      argumentsToString({
        hardPartition: true,
        multilayerSelfInterLinks: true,
        randomNodeCheckRate: 0.25,
        multilayerTest: 2,
        multilayerAggregation: true
      })
    ).toBe(
      " --multilayer-self-inter-links --hard-partition --random-node-check-rate 0.25 --multilayer-test 2 --multilayer-aggregation"
    );
  });
});
