import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";

const postMessage = vi.fn();
const terminate = vi.fn();

class WorkerMock {
  onmessage: ((event: MessageEvent) => void) | null = null;
  onerror: ((event: ErrorEvent) => void) | null = null;
  postMessage = postMessage;
  terminate = terminate;
}

vi.mock("../../src/worker", () => ({
  createInfomapWorker: () => new WorkerMock(),
}));

const { default: Infomap } = await import("../../src/index");

describe("Infomap", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.runOnlyPendingTimers();
    vi.useRealTimers();
    vi.clearAllMocks();
  });

  function getWorker(infomap: InstanceType<typeof Infomap>, id: number) {
    return (infomap as unknown as { workers: Record<number, WorkerMock> })
      .workers[id];
  }

  test("converts progress events from worker data output", () => {
    const progress = vi.fn();
    const infomap = new Infomap().on("progress", progress);
    const id = infomap.run({ network: "#source target\n1 2\n" });
    const worker = getWorker(infomap, id);

    worker.onmessage?.({
      data: {
        type: "data",
        content: "Trial 2/4",
      },
    } as MessageEvent);

    expect(postMessage).toHaveBeenCalled();
    expect(progress).toHaveBeenCalledWith(40, id);
  });

  test("reads the out name through a run of whitespace", () => {
    // The C++ tokenizer accepts "--out-name  mynet"; a regex matching a single space
    // missed it, fell back to the network basename, and then every read of the engine's
    // output landed on a file that was never written (#903).
    const infomap = new Infomap();
    const id = infomap.run({
      network: "1 2\n",
      args: "--out-name  mynet -o tree",
    });

    expect(getWorker(infomap, id).postMessage).toHaveBeenCalledWith(
      expect.objectContaining({ outName: "mynet" }),
    );
  });

  test("passes argv without empty entries", () => {
    // A shell never hands a program an empty argument. The rendered form of an
    // Arguments object starts with a space and the out name may be followed by a run
    // of whitespace, so splitting on a single space produced them (#903).
    const infomap = new Infomap();
    const fromObject = infomap.run({
      network: "1 2\n",
      args: { twoLevel: true, silent: true },
    });
    const fromString = infomap.run({
      network: "1 2\n",
      args: "--out-name  mynet  -o tree",
    });

    for (const id of [fromObject, fromString]) {
      const [[message]] = (
        getWorker(infomap, id).postMessage as unknown as {
          mock: { calls: [{ arguments: string[] }][] };
        }
      ).mock.calls.slice(-1);
      expect(message.arguments).not.toContain("");
      expect(message.arguments.every((arg) => arg.length > 0)).toBe(true);
    }
  });

  test("tells the worker whether to expect output files", () => {
    const infomap = new Infomap();
    const withFiles = infomap.run({ network: "1 2\n", args: "-o tree" });
    expect(getWorker(infomap, withFiles).postMessage).toHaveBeenCalledWith(
      expect.objectContaining({ expectsOutputFiles: true }),
    );

    const withoutFiles = infomap.run({
      network: "1 2\n",
      args: "--no-file-output",
    });
    expect(getWorker(infomap, withoutFiles).postMessage).toHaveBeenCalledWith(
      expect.objectContaining({ expectsOutputFiles: false }),
    );
  });

  test("converts pretty summary output to complete progress", () => {
    const progress = vi.fn();
    const infomap = new Infomap().on("progress", progress);
    const id = infomap.run({
      network: "#source target\n1 2\n",
      args: {},
    });
    const worker = getWorker(infomap, id);

    worker.onmessage?.({
      data: {
        type: "data",
        content: "Summary",
      },
    } as MessageEvent);

    expect(progress).toHaveBeenCalledWith(100, id);
  });

  test("runAsync still emits registered callbacks", async () => {
    const data = vi.fn();
    const progress = vi.fn();
    const finished = vi.fn();
    const infomap = new Infomap()
      .on("data", data)
      .on("progress", progress)
      .on("finished", finished);

    const resultPromise = infomap.runAsync({
      network: "#source target\n1 2\n",
    });
    const worker = getWorker(infomap, 0);

    worker.onmessage?.({
      data: {
        type: "data",
        content: "Trial 1/1",
      },
    } as MessageEvent);

    worker.onmessage?.({
      data: {
        type: "finished",
        content: { clu: "1 1\n2 1\n" },
      },
    } as MessageEvent);

    await expect(resultPromise).resolves.toEqual({ clu: "1 1\n2 1\n" });
    expect(data).toHaveBeenCalledWith("Trial 1/1", 0);
    expect(progress).toHaveBeenCalledWith(50, 0);
    expect(finished).toHaveBeenCalledWith({ clu: "1 1\n2 1\n" }, 0);
  });

  test("normalizes worker errors and terminates the failed worker", () => {
    const error = vi.fn();
    const preventDefault = vi.fn();
    const infomap = new Infomap().on("error", error);
    const id = infomap.run({ network: "#source target\n1 2\n" });
    const worker = getWorker(infomap, id);

    worker.onerror?.({
      message: "Error: failed to parse network",
      preventDefault,
    } as unknown as ErrorEvent);

    expect(preventDefault).toHaveBeenCalled();
    expect(error).toHaveBeenCalledWith("failed to parse network", id);
    expect(terminate).toHaveBeenCalledOnce();
  });

  test("terminates all active workers", async () => {
    const infomap = new Infomap();

    infomap.run({ network: "1 2\n" });
    infomap.run({ network: "2 3\n" });

    await expect(infomap.terminateAll(0)).resolves.toBe(2);
    expect(terminate).toHaveBeenCalledTimes(2);
  });
});
