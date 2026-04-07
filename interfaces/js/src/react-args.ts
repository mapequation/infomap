import type { Arguments } from "./arguments";
import type { RunOptions } from "./run-options";

type RunParams = [RunOptions];

export function mergeInfomapArgs(
  params: RunParams,
  args?: Arguments
): RunParams {
  const param = params[0] ? { ...params[0] } : {};

  if (typeof param.args === "object" && typeof args === "object") {
    param.args = { ...args, ...param.args };
  } else if (args !== undefined) {
    param.args = args;
  }

  return [param];
}
