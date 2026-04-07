import type { Arguments } from "./arguments";
import Infomap from "./index";
import { mergeInfomapArgs } from "./react-args";
import { useCallback, useDebugValue, useState } from "react";

export function useInfomap(args?: Arguments) {
  const [progress, setProgress] = useState(0);
  const [running, setRunning] = useState(false);
  const [infomap] = useState(() => new Infomap().on("progress", setProgress));

  const run = useCallback(
    (...params: Parameters<Infomap["run"]>) => {
      setRunning(true);
      setProgress(0);
      infomap.run(...mergeInfomapArgs(params, args));
      setRunning(false);
    },
    [infomap, args]
  );

  const runAsync = useCallback(
    (...params: Parameters<Infomap["runAsync"]>) => {
      setRunning(true);
      setProgress(0);
      return infomap
        .runAsync(...mergeInfomapArgs(params, args))
        .then((result) => {
          setRunning(false);
          return result;
        })
        .catch((error) => {
          setRunning(false);
          throw error;
        });
    },
    [infomap, args]
  );

  useDebugValue(running ? "Running" : "Stopped");

  return {
    infomap,
    progress,
    running,
    run,
    runAsync,
    on(...params: Parameters<Infomap["on"]>) {
      infomap.on(...params);
      return this;
    }
  };
}
