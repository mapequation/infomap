// @vitest-environment jsdom

import { act, createElement } from "react";
import { createRoot } from "react-dom/client";
import type { Root } from "react-dom/client";
import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";
import type { Arguments } from "../../src/arguments";
import type { UseInfomapOptions } from "../../src/react";

const postMessage = vi.fn();
const terminate = vi.fn();
const createdWorkers: WorkerMock[] = [];

class WorkerMock {
  onmessage: ((event: MessageEvent) => void) | null = null;
  onerror: ((event: ErrorEvent) => void) | null = null;
  postMessage = postMessage;
  terminate = terminate;

  constructor() {
    createdWorkers.push(this);
  }
}

vi.mock("../../src/worker", () => ({
  createInfomapWorker: () => new WorkerMock(),
}));

const { useInfomap } = await import("../../src/react");

type HookValue = ReturnType<typeof useInfomap>;

function HookHarness({
  args,
  onValue,
  options,
}: {
  args?: Arguments;
  onValue: (value: HookValue) => void;
  options?: UseInfomapOptions;
}) {
  const value = useInfomap(args, options);
  onValue(value);
  return null;
}

function renderUseInfomap(args?: Arguments, options?: UseInfomapOptions) {
  const container = document.createElement("div");
  const root: Root = createRoot(container);
  let current: HookValue | null = null;

  act(() => {
    root.render(
      createElement(HookHarness, {
        args,
        onValue(value) {
          current = value;
        },
        options,
      }),
    );
  });

  if (!current) throw new Error("Expected hook to render");

  return {
    get current() {
      if (!current) throw new Error("Expected hook value");
      return current;
    },
    unmount() {
      act(() => {
        root.unmount();
      });
    },
  };
}

function installAnimationFrame() {
  const frames = new Map<number, FrameRequestCallback>();
  let frameId = 0;

  Object.defineProperty(window, "requestAnimationFrame", {
    configurable: true,
    value: vi.fn((callback: FrameRequestCallback) => {
      frameId += 1;
      frames.set(frameId, callback);
      return frameId;
    }),
  });
  Object.defineProperty(window, "cancelAnimationFrame", {
    configurable: true,
    value: vi.fn((id: number) => {
      frames.delete(id);
    }),
  });

  return {
    flush() {
      const callbacks = [...frames.values()];
      frames.clear();
      act(() => {
        callbacks.forEach((callback) => callback(performance.now()));
      });
    },
    requestAnimationFrame: window.requestAnimationFrame as ReturnType<
      typeof vi.fn
    >,
    cancelAnimationFrame: window.cancelAnimationFrame as ReturnType<
      typeof vi.fn
    >,
  };
}

beforeEach(() => {
  // Fake only setTimeout/clearTimeout so leftover Infomap._terminate timers
  // (default 1000ms) do not fire across tests. Leave React's scheduler and
  // queueMicrotask alone so act()/rAF helpers continue to work.
  vi.useFakeTimers({ toFake: ["setTimeout", "clearTimeout"] });
});

afterEach(() => {
  vi.runOnlyPendingTimers();
  vi.useRealTimers();
  vi.clearAllMocks();
  createdWorkers.length = 0;
});

describe("useInfomap", () => {
  test("keeps running true until a run finishes", () => {
    const hook = renderUseInfomap();

    act(() => {
      expect(hook.current.run({ network: "1 2\n" })).toBe(0);
    });

    expect(hook.current.running).toBe(true);

    act(() => {
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "finished",
          content: { clu: "1 1\n2 1\n" },
        },
      } as MessageEvent);
    });

    expect(hook.current.running).toBe(false);
  });

  test("runs external callbacks without replacing hook state handlers", () => {
    const hook = renderUseInfomap();
    const finished = vi.fn();

    act(() => {
      hook.current.on("finished", finished);
      hook.current.run({ network: "1 2\n" });
    });

    act(() => {
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "finished",
          content: { clu: "1 1\n2 1\n" },
        },
      } as MessageEvent);
    });

    expect(finished).toHaveBeenCalledWith({ clu: "1 1\n2 1\n" }, 0);
    expect(hook.current.running).toBe(false);
  });

  test("normalizes errors and clears running state", () => {
    const hook = renderUseInfomap();
    const error = vi.fn();

    act(() => {
      hook.current.on("error", error);
      hook.current.run({ network: "1 2\n" });
    });

    act(() => {
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "error",
          content: "Error: failed to parse network",
        },
      } as MessageEvent);
    });

    expect(error).toHaveBeenCalledWith("failed to parse network", 0);
    expect(hook.current.running).toBe(false);
  });

  test("terminates active workers on unmount", () => {
    const hook = renderUseInfomap();

    act(() => {
      hook.current.run({ network: "1 2\n" });
    });

    hook.unmount();

    expect(terminate).toHaveBeenCalledOnce();
  });

  test("collects data events in one animation frame", () => {
    const animationFrame = installAnimationFrame();
    const hook = renderUseInfomap(undefined, { collectOutput: true });

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "first line",
        },
      } as MessageEvent);
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "second line",
        },
      } as MessageEvent);
    });

    expect(animationFrame.requestAnimationFrame).toHaveBeenCalledOnce();
    expect(hook.current.output).toEqual([]);

    animationFrame.flush();

    expect(hook.current.output).toEqual(["first line", "second line"]);
    expect(hook.current.outputText).toBe("first line\nsecond line");
  });

  test("clearOutput cancels pending output and clears collected output", () => {
    const animationFrame = installAnimationFrame();
    const hook = renderUseInfomap(undefined, { collectOutput: true });

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "pending line",
        },
      } as MessageEvent);
      hook.current.clearOutput();
    });

    expect(animationFrame.cancelAnimationFrame).toHaveBeenCalledOnce();
    animationFrame.flush();
    expect(hook.current.output).toEqual([]);
    expect(hook.current.outputText).toBe("");
  });

  test("run clears collected output by default", () => {
    const animationFrame = installAnimationFrame();
    const hook = renderUseInfomap(undefined, { collectOutput: true });

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "previous line",
        },
      } as MessageEvent);
    });
    animationFrame.flush();
    expect(hook.current.output).toEqual(["previous line"]);

    act(() => {
      hook.current.run({ network: "2 3\n" });
    });

    expect(hook.current.output).toEqual([]);
  });

  test("flushes pending output before finished callbacks", () => {
    installAnimationFrame();
    const finished = vi.fn();
    const hook = renderUseInfomap(undefined, {
      collectOutput: true,
      events: { finished },
    });

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "last line",
        },
      } as MessageEvent);
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "finished",
          content: { clu: "1 1\n2 1\n" },
        },
      } as MessageEvent);
    });

    expect(finished).toHaveBeenCalledWith({ clu: "1 1\n2 1\n" }, 0);
    expect(hook.current.output).toEqual(["last line"]);
  });

  test("flushes pending output before error callbacks", () => {
    installAnimationFrame();
    const error = vi.fn();
    const hook = renderUseInfomap(undefined, {
      collectOutput: true,
      events: { error },
    });

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "last line",
        },
      } as MessageEvent);
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "error",
          content: "Error: failed to parse network",
        },
      } as MessageEvent);
    });

    expect(error).toHaveBeenCalledWith("failed to parse network", 0);
    expect(hook.current.output).toEqual(["last line"]);
  });

  test("does not collect output unless requested", () => {
    const animationFrame = installAnimationFrame();
    const hook = renderUseInfomap();

    act(() => {
      hook.current.run({ network: "1 2\n" });
      createdWorkers[0]?.onmessage?.({
        data: {
          type: "data",
          content: "ignored line",
        },
      } as MessageEvent);
    });

    expect(animationFrame.requestAnimationFrame).not.toHaveBeenCalled();
    expect(hook.current.output).toEqual([]);
    expect(hook.current.outputText).toBe("");
  });
});
