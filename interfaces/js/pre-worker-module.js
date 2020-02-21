function readFile(filename) {
  var content = "";
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

var infomapWorkerId = -1;

var memoryHackRequest = {
  status: 200,
  useRequest: null,
  addEventListener: function(event, callback) {
    if (event === "load") {
      this.useRequest = callback;
    }
  }
};

var Module = {
  arguments: [],
  preRun: function() {
    addRunDependency("filesReady");
  },
  print: function(content) {
    postMessage({ type: "data", content, id: infomapWorkerId });
  },
  printErr: function(content) {
    postMessage({ type: "error", content, id: infomapWorkerId });
  },
  postRun: function() {
    var content = {};
    var clu = readFile("network.clu");
    if (clu) content.clu = clu;
    var tree = readFile("network.tree");
    if (tree) content.tree = tree;
    var ftree = readFile("network.ftree");
    if (ftree) content.ftree = ftree;
    postMessage({ type: "finished", content, id: infomapWorkerId });
  },
  memoryInitializerRequest: memoryHackRequest
};

onmessage = function onmessage(message) {
  var data = message.data;

  if (data.target === "Infomap") {
    memoryHackRequest.response = data.memBuffer;
    memoryHackRequest.useRequest();
    infomapWorkerId = data.id;
    var args = [data.inputFilename, "."];
    if (data.arguments) args = args.concat(data.arguments);
    Module.arguments.push(...args);
    FS.writeFile(data.inputFilename, data.inputData);
    removeRunDependency("filesReady");
  } else {
    throw "Unknown target on message to worker: " +
      JSON.stringify(data).substr(0, 150);
  }
};
