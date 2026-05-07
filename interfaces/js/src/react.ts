import type { Arguments } from "./arguments";
import Infomap, { type EventCallbacks } from "./index";
import { mergeInfomapArgs } from "./react-args";
import {
  useCallback,
  useDebugValue,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";

export type UseInfomapOptions = {
  collectOutput?: boolean;
  clearOutputOnRun?: boolean;
  events?: EventCallbacks;
};

function requestOutputFrame(callback: () => void) {
  if (typeof window !== "undefined" && window.requestAnimationFrame) {
    return window.requestAnimationFrame(callback);
  }

  return globalThis.setTimeout(callback, 0);
}

function cancelOutputFrame(id: number) {
  if (typeof window !== "undefined" && window.cancelAnimationFrame) {
    window.cancelAnimationFrame(id);
  } else {
    globalThis.clearTimeout(id);
  }
}

export function useInfomap(args?: Arguments, options: UseInfomapOptions = {}) {
  const [progress, setProgress] = useState(0);
  const [running, setRunning] = useState(false);
  const [output, setOutput] = useState<string[]>([]);
  const callbacksRef = useRef<EventCallbacks>({});
  const eventsRef = useRef<EventCallbacks | undefined>(options.events);
  const collectOutputRef = useRef(options.collectOutput ?? false);
  const clearOutputOnRunRef = useRef(options.clearOutputOnRun ?? true);
  const activeRunsRef = useRef(0);
  const outputBufferRef = useRef<string[]>([]);
  const outputFrameRef = useRef<number | null>(null);

  eventsRef.current = options.events;
  collectOutputRef.current = options.collectOutput ?? false;
  clearOutputOnRunRef.current = options.clearOutputOnRun ?? true;

  const clearOutput = useCallback(() => {
    outputBufferRef.current = [];
    if (outputFrameRef.current !== null) {
      cancelOutputFrame(outputFrameRef.current);
      outputFrameRef.current = null;
    }
    setOutput([]);
  }, []);

  const flushOutput = useCallback(() => {
    outputFrameRef.current = null;
    const buffered = outputBufferRef.current;
    if (buffered.length === 0) return;
    outputBufferRef.current = [];
    setOutput((currentOutput) => [...currentOutput, ...buffered]);
  }, []);

  const flushPendingOutput = useCallback(() => {
    if (outputFrameRef.current !== null) {
      cancelOutputFrame(outputFrameRef.current);
      outputFrameRef.current = null;
    }
    const buffered = outputBufferRef.current;
    if (buffered.length === 0) return;
    outputBufferRef.current = [];
    setOutput((currentOutput) => [...currentOutput, ...buffered]);
  }, []);

  const queueOutput = useCallback(
    (data: string) => {
      if (!collectOutputRef.current) return;
      outputBufferRef.current.push(data);
      if (outputFrameRef.current !== null) return;
      outputFrameRef.current = requestOutputFrame(flushOutput);
    },
    [flushOutput],
  );

  const emitCallback = useCallback(
    <Event extends keyof EventCallbacks>(
      event: Event,
      ...params: Parameters<Required<EventCallbacks>[Event]>
    ) => {
      const eventCallback = eventsRef.current?.[event] as
        | ((...args: Parameters<Required<EventCallbacks>[Event]>) => void)
        | undefined;
      const registeredCallback = callbacksRef.current[event] as
        | ((...args: Parameters<Required<EventCallbacks>[Event]>) => void)
        | undefined;

      eventCallback?.(...params);
      if (registeredCallback && registeredCallback !== eventCallback) {
        registeredCallback(...params);
      }
    },
    [],
  );

  const startRun = useCallback(() => {
    activeRunsRef.current += 1;
    setProgress(0);
    if (collectOutputRef.current && clearOutputOnRunRef.current) {
      clearOutput();
    }
    setRunning(true);
  }, [clearOutput]);

  const finishRun = useCallback(() => {
    activeRunsRef.current = Math.max(0, activeRunsRef.current - 1);
    setRunning(activeRunsRef.current > 0);
  }, []);

  const [infomap] = useState(() =>
    new Infomap()
      .on("data", (output, id) => {
        queueOutput(output);
        emitCallback("data", output, id);
      })
      .on("progress", (currentProgress, id) => {
        setProgress(currentProgress);
        emitCallback("progress", currentProgress, id);
      })
      .on("error", (message, id) => {
        flushPendingOutput();
        finishRun();
        emitCallback("error", message, id);
      })
      .on("finished", (result, id) => {
        flushPendingOutput();
        finishRun();
        emitCallback("finished", result, id);
      }),
  );

  useEffect(
    () => () => {
      activeRunsRef.current = 0;
      if (outputFrameRef.current !== null) {
        cancelOutputFrame(outputFrameRef.current);
        outputFrameRef.current = null;
      }
      void infomap.terminateAll(0);
    },
    [infomap],
  );

  const run = useCallback(
    (...params: Parameters<Infomap["run"]>) => {
      startRun();
      try {
        return infomap.run(...mergeInfomapArgs(params, args));
      } catch (error) {
        finishRun();
        throw error;
      }
    },
    [infomap, args, finishRun, startRun],
  );

  const runAsync = useCallback(
    (...params: Parameters<Infomap["runAsync"]>) => {
      startRun();
      try {
        return infomap.runAsync(...mergeInfomapArgs(params, args));
      } catch (error) {
        finishRun();
        throw error;
      }
    },
    [infomap, args, finishRun, startRun],
  );

  useDebugValue(running ? "Running" : "Stopped");
  const outputText = useMemo(() => output.join("\n"), [output]);

  return {
    infomap,
    output,
    outputText,
    progress,
    running,
    clearOutput,
    run,
    runAsync,
    on(...params: Parameters<Infomap["on"]>) {
      const [event, callback] = params;
      callbacksRef.current = { ...callbacksRef.current, [event]: callback };
      return this;
    },
  };
}
