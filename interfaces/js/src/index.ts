import argumentsToString from "./arguments";
import fileToString, {
  type TreeNode as Node,
  type TreeStateNode as StateNode,
} from "./filetypes";
import networkToString from "./network";
import type { RunOptions } from "./run-options";
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

export interface EventCallbacks {
  data?: (output: string, id: number) => void;
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
  | Event<"progress">
  | Event<"error">
  | Event<"finished">;

function normalizeErrorMessage(message: string) {
  return message.replace(/^(?:Error:\s*)+/i, "");
}

class Infomap {
  static __version__: string = packageJson.version;

  protected events: EventCallbacks = {};
  protected workerId = 0;
  protected workers: { [id: number]: Worker } = {};

  run(...args: Parameters<Infomap["createWorker"]>) {
    const id = this.createWorker(...args);
    this.setHandlers(id);
    return id;
  }

  runAsync(...args: Parameters<Infomap["createWorker"]>) {
    const id = this.createWorker(...args);
    return new Promise<Result>((finished, error) =>
      this.setHandlers(id, { finished, error }),
    );
  }

  on<E extends keyof EventCallbacks>(event: E, callback: EventCallbacks[E]) {
    this.events[event] = callback;
    return this;
  }

  protected createWorker({ network, filename, args, files }: RunOptions) {
    network = network ?? "";
    filename = filename ?? "network.net";
    args = args ?? "";
    files = files ?? {};

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

    worker.postMessage({
      arguments: args.split(" "),
      filename,
      network,
      outName,
      files,
    });

    return id;
  }

  protected setHandlers(id: number, events: EventCallbacks = {}) {
    const worker = this.workers[id];

    const emitData = (output: string) => {
      this.events.data?.(output, id);
      if (events.data && events.data !== this.events.data)
        events.data(output, id);
    };

    const emitProgress = (currentProgress: number) => {
      this.events.progress?.(currentProgress, id);
      if (events.progress && events.progress !== this.events.progress) {
        events.progress(currentProgress, id);
      }
    };

    const emitError = (message: string) => {
      const normalized = normalizeErrorMessage(message);
      this.events.error?.(normalized, id);
      if (events.error && events.error !== this.events.error) {
        events.error(normalized, id);
      }
    };

    const emitFinished = (result: Result) => {
      this.events.finished?.(result, id);
      if (events.finished && events.finished !== this.events.finished) {
        events.finished(result, id);
      }
    };

    worker.onmessage = (event: MessageEvent<EventData>) => {
      if (event.data.type === "data") {
        emitData(event.data.content);
        const match = event.data.content.match(/^Trial (\d+)\/(\d+)/);
        if (match) {
          const trial = Number(match[1]);
          const totTrials = Number(match[2]);
          emitProgress((100 * trial) / (totTrials + 1));
        } else {
          const summary = event.data.content.match(/^Summary after/);
          if (summary) {
            emitProgress(100);
          }
        }
      } else if (event.data.type === "error") {
        void this._terminate(id, 0);
        emitError(event.data.content);
      } else if (event.data.type === "finished") {
        void this._terminate(id);
        emitFinished(event.data.content);
      }
    };

    worker.onerror = (err: ErrorEvent) => {
      err.preventDefault();
      void this._terminate(id, 0);
      emitError(err.message);
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
    });
  }

  // Returns true if the worker with `id` existed and was terminated.
  async terminate(id: number, timeout = 1000) {
    return await this._terminate(id, timeout);
  }

  async terminateAll(timeout = 1000) {
    const ids = Object.keys(this.workers).map(Number);
    const terminated = await Promise.all(
      ids.map((id) => this._terminate(id, timeout)),
    );
    return terminated.filter(Boolean).length;
  }
}

export {
  Infomap as default,
  Infomap,
  changelog,
  parameters,
  networkToString,
  argumentsToString,
};

export {
  getResultFiles,
  getResultMetadata,
  resultFormats,
  type ResultFile,
  type ResultFormat,
  type ResultMetadata,
} from "./result";
