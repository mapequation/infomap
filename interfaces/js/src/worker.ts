import workerSource from "../../../build/js/infomap.worker.js";

let workerUrl: string | undefined;

export function createInfomapWorker() {
  workerUrl ??= URL.createObjectURL(
    new Blob([workerSource], { type: "application/javascript" })
  );
  return new Worker(workerUrl);
}
