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
});
