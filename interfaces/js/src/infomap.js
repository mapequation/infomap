class Infomap {
  static __version__ = VERSION;

  _events = {
    ondata: () => null,
    onerror: () => null,
    onfinished: () => null
  };

  run(network, args = "") {
    const worker = (this.worker = new Worker("Infomap-worker.js"));
    const defaultFilename = "network.net";

    if (typeof(network) !== "string") {
      throw new Error("network must be a string");
    }

    if (typeof(args) !== "string") {
      throw new Error("args must be a string");
    }

    worker.postMessage({
      target: "Infomap",
      inputFilename: defaultFilename,
      inputData: network,
      arguments: args.split()
    });

    worker.onmessage = this.onmessage;
    worker.onerror = err => err.preventDefault();

    return this;
  }

  on(event, callback) {
    if (event === "data") this._events.ondata = callback;
    else if (event === "error") this._events.onerror = callback;
    else if (event === "finished") this._events.onfinished = callback;
    else console.warn(`Unhandled event: ${event}`);

    return this;
  }

  onmessage = event => {
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

  cleanup() {
    if (this.worker && this.worker.terminate) {
      this.worker.terminate();
    }
    this.worker = null;
  }
}


export default Infomap;
