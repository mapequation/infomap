import { describe, expect, test } from "vitest";
import { mergeInfomapArgs } from "../../src/react-args";

describe("mergeInfomapArgs", () => {
  test("merges default object args with call-site args", () => {
    expect(
      mergeInfomapArgs([{ args: { silent: true } }], { twoLevel: true }),
    ).toEqual([{ args: { twoLevel: true, silent: true } }]);
  });

  test("uses default args when call-site args are missing", () => {
    expect(mergeInfomapArgs([{}], { tree: true })).toEqual([
      { args: { tree: true } },
    ]);
  });

  test("keeps a per-run string when the hook was given an object", () => {
    // Legal per the RunOptions type, and the per-run string used to be dropped
    // silently, running a configuration the caller never asked for (#903).
    expect(
      mergeInfomapArgs([{ args: "--two-level --num-trials 10" }], {
        silent: true,
      }),
    ).toEqual([{ args: "--silent --two-level --num-trials 10" }]);
  });

  test("gives the per-run string precedence over the hook default", () => {
    // Infomap's parser takes the last occurrence of an option, so the per-run
    // arguments come last -- the same precedence the object merge gives them.
    expect(
      mergeInfomapArgs([{ args: "--num-trials 10" }], { numTrials: 2 }),
    ).toEqual([{ args: "--num-trials 2 --num-trials 10" }]);
  });

  test("leaves a per-run string alone when the hook has no defaults", () => {
    expect(mergeInfomapArgs([{ args: "--two-level" }], undefined)).toEqual([
      { args: "--two-level" },
    ]);
  });
});
