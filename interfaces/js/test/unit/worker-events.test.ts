import { describe, expect, test, vi } from "vitest";

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
});
