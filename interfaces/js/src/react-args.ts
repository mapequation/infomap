import argumentsToString, { type Arguments } from "./arguments";
import type { RunOptions } from "./run-options";

type RunParams = [RunOptions];

/**
 * Combine the hook's default arguments with a single run's arguments.
 *
 * `RunOptions.args` is `string | Arguments`, so a call site can legally pass a string
 * to a hook created with an object. That combination used to drop the call site's
 * string and run with the hook's object alone -- no error, a result for a
 * configuration nobody asked for (#903). Mixed forms are now rendered to one command
 * line with the per-run arguments last, which gives them precedence the same way the
 * object merge does: Infomap's parser takes the last occurrence of an option.
 */
export function mergeInfomapArgs(
  params: RunParams,
  args?: Arguments,
): RunParams {
  const param = params[0] ? { ...params[0] } : {};

  if (args === undefined) {
    return [param];
  }

  if (param.args === undefined) {
    param.args = args;
    return [param];
  }

  if (typeof param.args === "object") {
    param.args = { ...args, ...param.args };
    return [param];
  }

  const perRun = param.args.trim();
  const defaults = argumentsToString(args).trim();
  param.args = [defaults, perRun].filter((part) => part.length > 0).join(" ");

  return [param];
}
