import { beforeEach, describe, expect, test, vi } from "vitest";

const postMessage = vi.fn();
const terminate = vi.fn();

class WorkerMock {
  onmessage: ((event: MessageEvent) => void) | null = null;
  onerror: ((event: ErrorEvent) => void) | null = null;
  postMessage = postMessage;
  terminate = terminate;
}

vi.mock("../../src/worker", () => ({
  createInfomapWorker: () => new WorkerMock()
}));

const { default: Infomap } = await import("../../src/index");

describe("Infomap", () => {
  beforeEach(() => {
    postMessage.mockClear();
    terminate.mockClear();
  });

  test("converts progress events from worker data output", () => {
    const progress = vi.fn();
    const infomap = new Infomap().on("progress", progress);
    const id = infomap.run({ network: "#source target\n1 2\n" });
    const worker = (
      infomap as unknown as { workers: Record<number, WorkerMock> }
    ).workers[id];

    worker.onmessage?.({
      data: {
        type: "data",
        content: "Trial 2/4"
      }
    } as MessageEvent);

    expect(postMessage).toHaveBeenCalled();
    expect(progress).toHaveBeenCalledWith(40, id);
  });

  test("dispatches structured events and jsonl output", () => {
    const onEvent = vi.fn();
    const onJsonl = vi.fn();
    const progress = vi.fn();
    const infomap = new Infomap()
      .on("event", onEvent)
      .on("jsonl", onJsonl)
      .on("progress", progress);
    const id = infomap.run({
      network: "#source target\n1 2\n",
      logFormat: "jsonl",
    });
    const worker = (
      infomap as unknown as { workers: Record<number, WorkerMock> }
    ).workers[id];

    worker.onmessage?.({
      data: {
        type: "event",
        content: {
          type: "trial_started",
          trial: 2,
          numTrials: 4,
          startedAt: "2026-04-13 10:00:00",
        },
      },
    } as MessageEvent);
    worker.onmessage?.({
      data: {
        type: "jsonl",
        content:
          '{"type":"trial_started","trial":2,"numTrials":4,"startedAt":"2026-04-13 10:00:00"}',
      },
    } as MessageEvent);
    worker.onmessage?.({
      data: {
        type: "event",
        content: {
          type: "summary",
          numTrials: 4,
          bestTrial: 2,
          numNodes: 3,
          numLinks: 2,
          averageDegree: 1.3,
          numTopModules: 2,
          numLevels: 2,
          oneLevelCodelength: 1.9,
          codelength: 1.5,
          relativeCodelengthSavings: 0.2,
        },
      },
    } as MessageEvent);

    expect(postMessage).toHaveBeenCalledWith(
      expect.objectContaining({ logFormat: "jsonl" })
    );
    expect(onEvent).toHaveBeenCalledTimes(2);
    expect(onEvent).toHaveBeenNthCalledWith(
      1,
      expect.objectContaining({ type: "trial_started", trial: 2 }),
      id
    );
    expect(onJsonl).toHaveBeenCalledWith(
      '{"type":"trial_started","trial":2,"numTrials":4,"startedAt":"2026-04-13 10:00:00"}',
      id
    );
    expect(progress).toHaveBeenNthCalledWith(1, 20, id);
    expect(progress).toHaveBeenNthCalledWith(2, 100, id);
  });
});
