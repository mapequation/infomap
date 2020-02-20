class Infomap {
  static __version__ = VERSION;

  _events = {
    ondata: () => null,
    onerror: () => null,
    onfinished: () => null
  };

  _workerId = 0;
  _workers = {};

  run(network, args = "") {
    if (typeof network !== "string") {
      throw new Error("network must be a string");
    }

    if (typeof args !== "string") {
      throw new Error("args must be a string");
    }

    const worker = new Worker("Infomap-worker.js");
    const id = this._workerId++;
    this._workers[id] = worker;
    const defaultFilename = "network.net";

    worker.postMessage({
      target: "Infomap",
      inputFilename: defaultFilename,
      inputData: network,
      arguments: args.split(),
      id
    });

    worker.onmessage = this.onmessage;
    worker.onerror = err => err.preventDefault();

    return id;
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
    const { type, content, id } = data;

    if (type === "data") {
      ondata(content, id);
    } else if (type === "error") {
      this.cleanup(id);
      onerror(content, id);
    } else if (type === "finished") {
      this.cleanup(id);
      onfinished(content, id);
    } else {
      throw new Error(
        `Unknown target on message from worker: ${JSON.stringify(data)}`
      );
    }
  };

  cleanup(id) {
    if (!this._workers[id]) return;

    const worker = this._workers[id];

    if (worker.terminate) {
      worker.terminate();
    }

    delete this._workers[id];
  }
}


export default Infomap;
