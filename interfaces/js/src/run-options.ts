import type { Arguments } from "./arguments";
import type { FileTypes } from "./filetypes";
import type { NetworkTypes } from "./network";

export type LogFormat = "text" | "jsonl" | "both";

export interface RunOptions {
  network?: string | NetworkTypes;
  filename?: string;
  args?: string | Arguments;
  files?: { [filename: string]: string | FileTypes };
  logFormat?: LogFormat;
}
