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
    ).toBe(" --output tree,clu --two-level --num-trials 5 -hh");
  });

  test("serializes variable Markov minimum scale", () => {
    expect(
      argumentsToString({
        variableMarkovMinScale: 0.5
      })
    ).toBe(" --variable-markov-min-scale 0.5");
  });
});
