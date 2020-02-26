const path = require("path");
const fsPromises = require("fs").promises;

async function getExampleNetworks(dirname) {
  const filenames = await fsPromises.readdir(dirname);
  const networks = {};
  for (let filename of filenames) {
    const content = await fsPromises.readFile(path.join(dirname, filename), {
      encoding: "utf8"
    });
    networks[filename] = content.replace(/\r?\n/g, "\n");
  }
  return networks;
}

module.exports = getExampleNetworks;
