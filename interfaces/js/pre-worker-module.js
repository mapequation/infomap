function readFile(filename) {
  let content = undefined;
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

var infomapWorkerId = -1;
var outName = "Untitled";

var memoryHackRequest = {
  status: 200,
  useRequest: null,
  addEventListener: function (event, callback) {
    if (event === "load") {
      this.useRequest = callback;
    }
  },
};

var Module = {
  arguments: [],
  preRun: function () {
    addRunDependency("filesReady");
  },
  print: function (content) {
    postMessage({ type: "data", content, id: infomapWorkerId });
  },
  printErr: function (content) {
    postMessage({ type: "error", content, id: infomapWorkerId });
  },
  postRun: function () {
    const content = {
      clu: readFile(`${outName}.clu`), // -o clu
      clu_states: readFile(`${outName}_states.clu`), // -o clu (for state networks)
      tree: readFile(`${outName}.tree`), // -o tree
      tree_states: readFile(`${outName}_states.tree`), // -o tree (for state networks)
      ftree: readFile(`${outName}.ftree`), // -o ftree
      ftree_states: readFile(`${outName}_states.ftree`), // -o ftree (for state networks)
      newick: readFile(`${outName}.nwk`), // -o newick
      newick_states: readFile(`${outName}_states.nwk`), // -o newick (for state networks)
      json: readFile(`${outName}.json`), // -o json
      json_states: readFile(`${outName}_states.json`), // -o json (for state networks)
      csv: readFile(`${outName}.csv`), // -o csv
      csv_states: readFile(`${outName}_states.csv`), // -o csv (for state networks)
      net: readFile(`${outName}.net`), // -o network (for state networks)
      states_as_physical: readFile(`${outName}_states_as_physical.net`), // -o network (for state networks)
      states: readFile(`${outName}_states.net`), // -o states
    };
    postMessage({ type: "finished", content, id: infomapWorkerId });
  },
  memoryInitializerRequest: memoryHackRequest,
};

onmessage = function onmessage(message) {
  const data = message.data;

  memoryHackRequest.response = data.memBuffer;
  memoryHackRequest.useRequest();
  infomapWorkerId = data.id;
  outName = data.outName;
  const args = [data.inputFilename, ".", ...data.arguments];
  Module.arguments.push(...args);
  FS.writeFile(data.inputFilename, data.inputData);
  for (let filename of Object.keys(data.files)) {
    FS.writeFile(filename, data.files[filename]);
  }
  removeRunDependency("filesReady");
};
