import { version } from "../../../package.json";


class Infomap {
  static __version__ = version;

  constructor() {
    this._events = {
      ondata: () => null,
      onerror: () => null,
      onfinished: () => null
    };
  }

  run(filename, data, args) {
    const worker = this.worker = new Worker("Infomap-worker.js");

    worker.postMessage({
      target: "Infomap",
      inputFilename: filename,
      inputData: data,
      args
    });

    worker.onmessage = this.onmessage();
    worker.onerror = err => err.preventDefault();

    return this;
  }

  on(event, callback) {
    if (event === "data") this._events.ondata = callback;
    if (event === "error") this._events.onerror = callback;
    if (event === "finished") this._events.onfinished = callback;

    return this;
  }

  onmessage() {
    return (event) => {
      const { ondata, onerror, onfinished } = this._events;
      const { data } = event;
      const { target } = data;

      if (target === "stdout") {
        ondata(data.content);
      } else if (target === "stderr") {
        this.worker = null;
        onerror(data.content);
      } else if (target === "finished") {
        this.cleanup();
        onfinished(data.output);
      } else {
        throw new Error(`Unknown target on message from worker: ${data}`);
      }
    };
  }

  cleanup() {
    if (this.worker && this.worker.terminate) {
      this.worker.terminate();
    }
    this.worker = null;
  }
}


export default Infomap;
