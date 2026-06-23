// `infomap` bin, published as the package's bin entry. Backed by the NODERAWFS
// Emscripten build, it forwards process arguments straight to main(), so it
// behaves like the native Infomap binary on real files:
//
//   npx @mapequation/infomap network.net . --tree
import createInfomapModule from "./infomap.cli.mjs";

const Module = await createInfomapModule({ noInitialRun: true });

try {
  const status = Module.callMain(process.argv.slice(2));
  process.exit(typeof status === "number" ? status : 0);
} catch (error) {
  // Emscripten throws an ExitStatus object when the program calls exit().
  if (error && typeof error === "object" && "status" in error) {
    process.exit(Number((error as { status: unknown }).status) || 0);
  }
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
