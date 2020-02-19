function readFile(filename) {
  var content = '';
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

var Module = {};

Module['preRun'] = function infomap_preRun() {
  console.log("Pre-run: adding 'filesReady' as run dependency...");
  addRunDependency('filesReady');
};

Module['postRun'] = function infomap_postRun() {
  console.log("Post-run: Read result...");
  var output = {};
  var clu = readFile("network.clu");
  if (clu) output.clu = clu;
  var tree = readFile("network.tree");
  if (tree) output.tree = tree;
  postMessage({ target: 'finished', output: output});
};

Module['print'] = function Module_print(x) {
  postMessage({ target: 'stdout', content: x });
};
Module['printErr'] = function Module_printErr(x) {
  postMessage({ target: 'stderr', content: x });
};

onmessage = function onmessage(message) {
  var data = message.data;
  if (data.target === "Infomap") {
    var args = [data.inputFilename, "."];
    if (data.arguments) args = args.concat(data.arguments);
    Module['arguments'] = args;
    FS.writeFile(data.inputFilename, data.inputData);
    removeRunDependency('filesReady');
  } else {
    throw 'Unknown target on message to worker: ' + JSON.stringify(data).substr(0, 150);
  }
};
