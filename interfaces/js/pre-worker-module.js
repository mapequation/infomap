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

var Module = {
  arguments: [],
  preRun: function () {
    addRunDependency("filesReady");
  },
  print: function (content) {
    postMessage({ type: "data", content });
  },
  printErr: function (content) {
    postMessage({ type: "error", content });
  },
  postRun: function () {
    const content = {};
    for (const file of resultFiles) {
      content[file.key] = readResultFile(file);
    }
    postMessage({ type: "finished", content });
  },
};

onmessage = function onmessage(message) {
  const data = message.data;
  outName = data.outName;
  Module.arguments.push(...[data.filename, ".", ...data.arguments]);
  FS.writeFile(data.filename, data.network);
  for (let filename of Object.keys(data.files)) {
    FS.writeFile(filename, data.files[filename]);
  }
  removeRunDependency("filesReady");
};
