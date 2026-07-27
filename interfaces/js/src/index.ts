import argumentsToString from "./arguments";
import fileToString, {
  type TreeNode as Node,
  type TreeStateNode as StateNode,
} from "./filetypes";
import networkToString from "./network";
export type {
  InfomapNetworkJson,
  InfomapNetworkJsonNode,
  InfomapNetworkJsonState,
  InfomapNetworkJsonEdge,
  InfomapNetworkJsonType,
} from "./network";
import type { RunOptions } from "./run-options";
import { createInfomapWorker } from "./worker";
import parameters from "../generated/parameters.json";
import packageJson from "../../../package.json";

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

// Node fields stay optional, because the JSON output omits them: a higher-order
// network's physical JSON carries no `modules` at all, and `stateId`/`layerId` are not
// in it either. Wrapping these in `Required` typed them as always present, so consumer
// code dereferenced undefined with no compile error (#903). Measured on 2.15.0: nodes
// of a first-order network have modules, of states.net and multilayer.net they do not.
export type Tree<NodeType = Node> = Header & {
  nodes: NodeType[];
  modules: Module[];
};

export type StateTree = Tree<StateNode>;

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
    // \s+ rather than \s: the C++ tokenizer accepts a run of whitespace, so
    // "--out-name  mynet" is a valid command line. Matching a single space made the
    // regex miss, the basename fall back to the network name, and every read of the
    // engine's output land on a file that was never written (#903).
    const outNameMatch = args.match(/--out-name\s+(\S+)/);
    const outName = outNameMatch?.[1] ? outNameMatch[1] : networkName;
    // Whether output files are expected at all, so the worker can tell "the engine
    // wrote nothing" from "we looked in the wrong place".
    const expectsOutputFiles = !/(?:^|\s)--no-file-output(?:\s|$)/.test(args);

    const worker = createInfomapWorker();
    const id = this.workerId++;
    this.workers[id] = worker;

    worker.postMessage({
      arguments: args.split(" "),
      filename,
      network,
      outName,
      files,
      expectsOutputFiles,
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
      // Settle the runAsync promise first so a throwing global callback
      // cannot leave the returned promise pending forever.
      if (events.error && events.error !== this.events.error) {
        events.error(normalized, id);
      }
      try {
        this.events.error?.(normalized, id);
      } catch {
        // user-registered global callback threw; promise has already settled
      }
    };

    const emitFinished = (result: Result) => {
      if (events.finished && events.finished !== this.events.finished) {
        events.finished(result, id);
      }
      try {
        this.events.finished?.(result, id);
      } catch {
        // user-registered global callback threw; promise has already settled
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
          const summary = event.data.content.match(/^Summary(?: after)?/);
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
