function readFile(filename) {
  var content = "";
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

var Module = {};
var id;

Module["preRun"] = function infomap_preRun() {
  addRunDependency("filesReady");
};

Module["postRun"] = function infomap_postRun() {
  var content = {};
  var clu = readFile("network.clu");
  if (clu) content.clu = clu;
  var tree = readFile("network.tree");
  if (tree) content.tree = tree;
  var ftree = readFile("network.ftree");
  if (ftree) content.ftree = ftree;
  postMessage({ type: "finished", content: content, id: id });
};

Module["print"] = function Module_print(x) {
  postMessage({ type: "stdout", content: x, id: id });
};
Module["printErr"] = function Module_printErr(x) {
  postMessage({ type: "stderr", content: x, id: id });
};

onmessage = function onmessage(message) {
  var data = message.data;
  id = data.id;
  if (data.target === "Infomap") {
    var args = [data.inputFilename, "."];
    if (data.arguments) args = args.concat(data.arguments);
    Module["arguments"] = args;
    FS.writeFile(data.inputFilename, data.inputData);
    removeRunDependency("filesReady");
  } else {
    throw "Unknown target on message to worker: " +
      JSON.stringify(data).substr(0, 150);
  }
};
