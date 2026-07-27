function readFile(filename) {
  let content = undefined;
  try {
    content = FS.readFile(filename, { encoding: "utf8" });
  } catch (e) {}
  return content;
}

function readResultFile(file) {
  let content = readFile(`${outName}${file.suffix}.${file.extension}`);
  if (file.extension !== "json" || !content) {
    return content;
  }

  try {
    content = JSON.parse(content);
  } catch (err) {
    postMessage({ type: "error", content: err.message });
    content = undefined;
  }
  return content;
}

let outName = "Untitled";
let expectsOutputFiles = true;
let pendingMessage = undefined;
let readyForFiles = false;

// The input network lives in its own directory so it cannot be mistaken for output.
// Written next to the output it was: the out name is the input filename minus its
// extension, so the "net" result key resolved to the input file and every default run
// handed the caller its own network back under a key documented as Infomap's Pajek
// output (#903).
const INPUT_DIR = "infomap-input";

function prepareFiles(message) {
  const data = message.data;
  outName = data.outName;
  expectsOutputFiles = data.expectsOutputFiles !== false;
  try {
    FS.mkdir(INPUT_DIR);
  } catch (e) {
    // already created by an earlier run in this worker
  }
  const inputPath = `${INPUT_DIR}/${data.filename}`;
  Module.arguments.push(...[inputPath, ".", ...data.arguments]);
  FS.writeFile(inputPath, data.network);
  for (let filename of Object.keys(data.files)) {
    FS.writeFile(filename, data.files[filename]);
  }
  removeRunDependency("filesReady");
}

function processPendingMessage() {
  if (readyForFiles && pendingMessage) {
    const message = pendingMessage;
    pendingMessage = undefined;
    prepareFiles(message);
  }
}

var Module = {
  arguments: [],
  preRun: function () {
    addRunDependency("filesReady");
    readyForFiles = true;
    processPendingMessage();
  },
  print: function (content) {
    postMessage({ type: "data", content });
  },
  printErr: function (content) {
    postMessage({ type: "error", content });
  },
  postRun: function () {
    const content = {};
    let found = 0;
    for (const file of resultFiles) {
      content[file.key] = readResultFile(file);
      if (content[file.key] !== undefined) {
        found++;
      }
    }
    // "The engine wrote files we then failed to find" used to be indistinguishable from
    // "the engine produced nothing": every read missed, readResultFile swallowed it, and
    // the run resolved with an empty result (#903).
    if (found === 0 && expectsOutputFiles) {
      postMessage({
        type: "error",
        content:
          `Infomap finished but none of its output files were found under "${outName}". ` +
          "This usually means --out-name and the name the results are read back under " +
          "disagree. Pass --no-file-output if no output files are wanted.",
      });
      return;
    }
    postMessage({ type: "finished", content });
  },
};

onmessage = function onmessage(message) {
  pendingMessage = message;
  processPendingMessage();
};
