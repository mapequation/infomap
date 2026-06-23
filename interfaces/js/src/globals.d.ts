declare module "*.worker.js" {
  const source: string;
  export default source;
}

// Minimal surface of the Emscripten MODULARIZE factory emitted by
// `make build-node` (build/js/infomap.node.mjs and infomap.cli.mjs). The wasm is
// inlined (SINGLE_FILE), main() is driven via callMain, and FS is the in-memory
// filesystem used to stage networks and read results for the library build.
interface InfomapEmscriptenFS {
  mkdir(path: string): void;
  writeFile(path: string, data: string | ArrayBufferView): void;
  readFile(path: string, options: { encoding: "utf8" }): string;
  readdir(path: string): string[];
}

interface InfomapEmscriptenModule {
  callMain(args: string[]): number | undefined;
  FS: InfomapEmscriptenFS;
}

interface InfomapModuleOptions {
  noInitialRun?: boolean;
  print?: (line: string) => void;
  printErr?: (line: string) => void;
}

type InfomapModuleFactory = (
  options?: InfomapModuleOptions,
) => Promise<InfomapEmscriptenModule>;

declare module "*.node.mjs" {
  const createModule: InfomapModuleFactory;
  export default createModule;
}

declare module "*.cli.mjs" {
  const createModule: InfomapModuleFactory;
  export default createModule;
}
