import { describe, expect, test } from "vitest";
import { mergeInfomapArgs } from "../../src/react-args";

describe("mergeInfomapArgs", () => {
  test("merges default object args with call-site args", () => {
    expect(
      mergeInfomapArgs([{ args: { silent: true } }], { twoLevel: true })
    ).toEqual([{ args: { twoLevel: true, silent: true } }]);
  });

  test("uses default args when call-site args are missing", () => {
    expect(mergeInfomapArgs([{}], { tree: true })).toEqual([
      { args: { tree: true } },
    ]);
  });
});
