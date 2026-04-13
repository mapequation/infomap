import argumentsToString from "./arguments";
import fileToString, {
  type TreeNode as Node,
  type TreeStateNode as StateNode
} from "./filetypes";
import networkToString from "./network";
import type { LogFormat, RunOptions } from "./run-options";
import { createInfomapWorker } from "./worker";
import changelog from "../generated/changelog.json";
import parameters from "../generated/parameters.json";
import packageJson from "../../../package.json";

export interface Changelog {
  body: string | null;
  date: string;
  footer: string | null;
  header: string;
  mentions: string[];
  merge: string | null;
  notes: string[];
  references: string[];
  revert: string | null;
  scope: string | null;
  subject: string;
  type: string | null;
}

export interface Parameter<Required = false> {
  long: string;
  short: string;
  description: string;
  group: string;
  required: Required;
  advanced: boolean;
  incremental: boolean;
  default: boolean | string | number;
}

export interface RequiredParameter extends Parameter<true> {
  longType: string;
  shortType: string;
  default: string;
}

export type Module = {
  path: number[];
  enterFlow: number;
  exitFlow: number;
  numEdges: number;
  numChildren: number;
  codelength: number;
  links?: {
    source: number;
    target: number;
    flow: number;
  }[];
};

export interface Header {
  version: string;
  args: string;
  startedAt: string;
  completedIn: number;
  codelength: number;
  numLevels: number;
  numTopModules: number;
  relativeCodelengthSavings: number;
  directed: boolean;
  flowModel: string;
  higherOrder: boolean;
  stateLevel?: boolean;
  bipartiteStartId?: number;
}

export type Tree<NodeType = Required<Node>> = Header & {
  nodes: NodeType[];
  modules: Module[];
};

export type StateTree = Tree<Required<StateNode>>;

export interface Result {
  clu?: string;
  clu_states?: string;
  tree?: string;
  tree_states?: string;
  ftree?: string;
  ftree_states?: string;
  newick?: string;
  newick_states?: string;
  json?: Tree;
  json_states?: StateTree;
  csv?: string;
  csv_states?: string;
  net?: string;
  states_as_physical?: string;
  states?: string;
  flow?: string;
  flow_as_physical?: string;
}

export interface RunStartedEvent {
  type: "run_started";
  version: string;
  startedAt: string;
  inputNetwork: string;
  outputPath: string;
  arguments: string;
  numTrials: number;
  noFileOutput: boolean;
  initialPartitionCount: number;
}

export interface LogLineEvent {
  type: "log_line";
  message: string;
}

export interface TrialStartedEvent {
  type: "trial_started";
  trial: number;
  numTrials: number;
  startedAt: string;
}

export interface TrialFinishedEvent {
  type: "trial_finished";
  trial: number;
  numTrials: number;
  elapsedSeconds: number;
  codelength: number;
  numTopModules: number;
}

export interface SummaryEvent {
  type: "summary";
  numTrials: number;
  bestTrial: number;
  numNodes: number;
  numLinks: number;
  averageDegree: number;
  numTopModules: number;
  numLevels: number;
  oneLevelCodelength: number;
  codelength: number;
  relativeCodelengthSavings: number;
  minCodelength?: number;
  averageCodelength?: number;
  maxCodelength?: number;
  minNumTopModules?: number;
  averageNumTopModules?: number;
  maxNumTopModules?: number;
}

export interface PartitionResultEvent {
  type: "partition_result";
  codelength: number;
  numTopModules: number;
  numNonTrivialTopModules: number;
}

export interface WarningEvent {
  type: "warning";
  message: string;
}

export interface RunFailedEvent {
  type: "run_failed";
  message: string;
}

export interface RunFinishedEvent {
  type: "run_finished";
  endedAt: string;
  elapsedSeconds: number;
}

export type InfomapRunEvent =
  | RunStartedEvent
  | LogLineEvent
  | TrialStartedEvent
  | TrialFinishedEvent
  | SummaryEvent
  | PartitionResultEvent
  | WarningEvent
  | RunFailedEvent
  | RunFinishedEvent;

export interface EventCallbacks {
  data?: (output: string, id: number) => void;
  event?: (event: InfomapRunEvent, id: number) => void;
  jsonl?: (line: string, id: number) => void;
  progress?: (progress: number, id: number) => void;
  error?: (message: string, id: number) => void;
  finished?: (result: Result, id: number) => void;
}

interface Event<Type extends keyof EventCallbacks> {
  type: Type;
  content: Parameters<Required<EventCallbacks>[Type]>[0];
}

type EventData =
  | Event<"data">
  | Event<"event">
  | Event<"jsonl">
  | Event<"progress">
  | Event<"error">
  | Event<"finished">;

function getLegacyProgress(output: string) {
  const match = output.match(/^Trial (\d+)\/(\d+)/);
  if (match) {
    const trial = Number(match[1]);
    const totalTrials = Number(match[2]);
    return (100 * trial) / (totalTrials + 1);
  }

  if (output.match(/^Summary after/)) {
    return 100;
  }

  return undefined;
}

function getStructuredProgress(event: InfomapRunEvent) {
  switch (event.type) {
    case "run_started":
      return 0;
    case "trial_started":
      return (100 * (event.trial - 1)) / (event.numTrials + 1);
    case "trial_finished":
      return (100 * event.trial) / (event.numTrials + 1);
    case "summary":
    case "run_finished":
      return 100;
    default:
      return undefined;
  }
}

class Infomap {
  static __version__: string = packageJson.version;

  protected events: EventCallbacks = {};
  protected workerId = 0;
  protected workers: { [id: number]: Worker } = {};
  protected workerFormats: { [id: number]: LogFormat } = {};

  run(...args: Parameters<Infomap["createWorker"]>) {
    const id = this.createWorker(...args);
    this.setHandlers(id);
    return id;
  }

  runAsync(...args: Parameters<Infomap["createWorker"]>) {
    const id = this.createWorker(...args);
    return new Promise<Result>((finished, error) =>
      this.setHandlers(id, { finished, error })
    );
  }

  on<E extends keyof EventCallbacks>(event: E, callback: EventCallbacks[E]) {
    this.events[event] = callback;
    return this;
  }

  protected createWorker({
    network,
    filename,
    args,
    files,
    logFormat,
  }: RunOptions) {
    network = network ?? "";
    filename = filename ?? "network.net";
    args = args ?? "";
    files = files ?? {};
    logFormat = logFormat ?? "text";

    if (typeof network !== "string") {
      network = networkToString(network);
    }

    if (typeof args !== "string") {
      args = argumentsToString(args);
    }

    for (const key of Object.keys(files)) {
      const file = files[key];
      if (typeof file !== "string") {
        files[key] = fileToString(file);
      }
    }

    const index = filename.lastIndexOf(".");
    const networkName = index > 0 ? filename.slice(0, index) : filename;
    const outNameMatch = args.match(/--out-name\s(\S+)/);
    const outName =
      outNameMatch && outNameMatch[1] ? outNameMatch[1] : networkName;

    const worker = createInfomapWorker();
    const id = this.workerId++;
    this.workers[id] = worker;
    this.workerFormats[id] = logFormat;

    worker.postMessage({
      arguments: args.split(" "),
      filename,
      network,
      outName,
      files,
      logFormat,
    });

    return id;
  }

  protected setHandlers(id: number, events = this.events) {
    const worker = this.workers[id];
    const format = this.workerFormats[id] ?? "text";
    const { data, event: onEvent, jsonl, progress, error, finished } = {
      ...this.events,
      ...events,
    };

    worker.onmessage = (event: MessageEvent<EventData>) => {
      if (event.data.type === "data") {
        if (data) {
          data(event.data.content, id);
        }
        if (progress && format === "text") {
          const currentProgress = getLegacyProgress(event.data.content);
          if (currentProgress != null) {
            progress(currentProgress, id);
          }
        }
      } else if (event.data.type === "event") {
        if (onEvent) {
          onEvent(event.data.content, id);
        }
        if (progress && format !== "text") {
          const currentProgress = getStructuredProgress(event.data.content);
          if (currentProgress != null) {
            progress(currentProgress, id);
          }
        }
      } else if (event.data.type === "jsonl") {
        if (jsonl) {
          jsonl(event.data.content, id);
        }
      } else if (error && event.data.type === "error") {
        void this._terminate(id);
        error(event.data.content, id);
      } else if (finished && event.data.type === "finished") {
        void this._terminate(id);
        finished(event.data.content, id);
      }
    };

    worker.onerror = (err: ErrorEvent) => {
      err.preventDefault();
      if (error) error(err.message, id);
    };
  }

  protected _terminate(id: number, timeout = 1000): Promise<boolean> {
    if (!this.workers[id]) return Promise.resolve(false);

    const worker = this.workers[id];

    return new Promise<boolean>((resolve) => {
      const terminate = () => {
        worker.terminate();
        resolve(true);
      };

      if (timeout <= 0) {
        terminate();
      } else {
        setTimeout(terminate, timeout);
      }

      delete this.workers[id];
      delete this.workerFormats[id];
    });
  }

  // Returns true if the worker with `id` existed and was terminated.
  async terminate(id: number, timeout = 1000) {
    return await this._terminate(id, timeout);
  }
}

export {
  Infomap as default,
  Infomap,
  changelog,
  parameters,
  networkToString,
  argumentsToString
};
