// var arguments = ["network.txt", "-z", '--clu', "."];
var Module = {};

Module['preRun'] = function infomap_preRun() {
  console.log("Pre-run: adding 'filesReady' as run dependency...");
  addRunDependency('filesReady');
};

Module['postRun'] = function infomap_postRun() {
  console.log("Post-run: Read result...");
  var output = {};
  var args = Module['arguments'];
  // if (args.indexOf('--clu') != -1)
  //   output.clu = FS.readFile("network.clu", {encoding: "utf8"});
  // else
  //   output.tree = FS.readFile("network.tree", {encoding: "utf8"});
  var clu = readFile("network.clu");
  if (clu)
    output.clu = clu;
  var tree = readFile("network.tree");
  if (tree)
    output.tree = tree;
  postMessage({ target: 'finished', output: output});
};

Module['print'] = function Module_print(x) {
  //dump('OUT: ' + x + '\n');
  postMessage({ target: 'stdout', content: x });
};
Module['printErr'] = function Module_printErr(x) {
  //dump('ERR: ' + x + '\n');
  postMessage({ target: 'stderr', content: x });
};

function readFile(filename) {
  var content = '';
  try {
    content = FS.readFile(filename, {encoding: "utf8"});
  } catch (e) {}
  return content;
}

onmessage = function onmessage(message) {
  //dump('worker got ' + JSON.stringify(message.data).substr(0, 150) + '\n');
  var data = message.data;
  switch (data.target) {
    case 'Infomap': {
      var args = [data.inputFilename, "."];
      if (data.arguments)
        args = args.concat(data.arguments);
      // Module.print("Worker got 'Infomap' with arguments: " + args);
      Module['arguments'] = args;
      // Module.print("Writing file '" + data.inputFilename + "' with data:\n" + data.inputData);
      FS.writeFile(data.inputFilename, data.inputData);
      removeRunDependency('filesReady');
      break;
    }
    default:
      throw 'Unknown target on message to worker: ' + JSON.stringify(data).substr(0, 150);
  }
};
