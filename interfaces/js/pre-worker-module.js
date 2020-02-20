function readFile(filename) {
  var content = "";
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

var Module = {
  arguments: [],
  preRun: function() {
    addRunDependency("filesReady");
  },
  print: function(content) {
    postMessage({ type: "data", content });
  },
  printErr: function(content) {
    postMessage({ type: "error", content });
  },
  postRun: function() {
    var content = {};
    var clu = readFile("network.clu");
    if (clu) content.clu = clu;
    var tree = readFile("network.tree");
    if (tree) content.tree = tree;
    var ftree = readFile("network.ftree");
    if (ftree) content.ftree = ftree;
    postMessage({ type: "finished", content });
  }
};

onmessage = function onmessage(message) {
  var data = message.data;

  if (data.target === "Infomap") {
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
