function readFile(filename) {
  let content = undefined;
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

let outName = "Untitled";
let logFormat = "text";

function shouldEmitText() {
  return logFormat === "text" || logFormat === "both";
}

function shouldEmitStructured() {
  return logFormat === "jsonl" || logFormat === "both";
}

function shouldEmitJsonl() {
  return logFormat === "jsonl" || logFormat === "both";
}

function emitStructured(event) {
  if (!shouldEmitStructured()) {
    return;
  }

  postMessage({ type: "event", content: event });

  if (shouldEmitJsonl()) {
    postMessage({ type: "jsonl", content: JSON.stringify(event) });
  }
}

globalThis.__infomapEmitStructuredEvent = function (type, payloadJson) {
  if (!shouldEmitStructured()) {
    return;
  }

  let payload = {};

  if (payloadJson && payloadJson !== "{}") {
    try {
      payload = JSON.parse(payloadJson);
    } catch (error) {
      postMessage({
        type: "error",
        content: `Failed to parse structured Infomap event payload: ${error.message}`,
      });
      return;
    }
  }

  emitStructured({ type, ...payload });
};

var Module = {
  arguments: [],
  preRun: function () {
    addRunDependency("filesReady");
  },
  print: function (content) {
    if (shouldEmitText()) {
      postMessage({ type: "data", content });
    }
    emitStructured({ type: "log_line", message: content });
  },
  printErr: function (content) {
    postMessage({ type: "error", content });
  },
  postRun: function () {
    let json = readFile(`${outName}.json`); // -o json
    let json_states = readFile(`${outName}_states.json`); // -o json (for state networks)

    if (json) {
      try {
        json = JSON.parse(json);
      } catch (err) {
        postMessage({ type: "error", content: err.message });
        json = undefined;
      }
    }

    if (json_states) {
      try {
        json_states = JSON.parse(json_states);
      } catch (err) {
        postMessage({ type: "error", content: err.message });
        json_states = undefined;
      }
    }

    const content = {
      clu: readFile(`${outName}.clu`), // -o clu
      clu_states: readFile(`${outName}_states.clu`), // -o clu (for state networks)
      tree: readFile(`${outName}.tree`), // -o tree
      tree_states: readFile(`${outName}_states.tree`), // -o tree (for state networks)
      ftree: readFile(`${outName}.ftree`), // -o ftree
      ftree_states: readFile(`${outName}_states.ftree`), // -o ftree (for state networks)
      newick: readFile(`${outName}.nwk`), // -o newick
      newick_states: readFile(`${outName}_states.nwk`), // -o newick (for state networks)
      json,
      json_states,
      csv: readFile(`${outName}.csv`), // -o csv
      csv_states: readFile(`${outName}_states.csv`), // -o csv (for state networks)
      net: readFile(`${outName}.net`), // -o network (for state networks)
      states_as_physical: readFile(`${outName}_states_as_physical.net`), // -o network (for state networks)
      states: readFile(`${outName}_states.net`), // -o states
      flow: readFile(`${outName}_flow.net`), // -o flow
      flow_as_physical: readFile(`${outName}_states_as_physical_flow.net`), // -o flow
    };
    postMessage({ type: "finished", content });
  },
};

onmessage = function onmessage(message) {
  const data = message.data;
  outName = data.outName;
  logFormat = data.logFormat || "text";
  Module.arguments.push(...[data.filename, ".", ...data.arguments]);
  FS.writeFile(data.filename, data.network);
  for (let filename of Object.keys(data.files)) {
    FS.writeFile(filename, data.files[filename]);
  }
  removeRunDependency("filesReady");
};
