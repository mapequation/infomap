import path from "node:path";
import { defineConfig } from "@playwright/test";

const port = 4173;
const rootDir = path.resolve(import.meta.dirname, "../..");
const python = process.env.PYTHON ?? "python3";

export default defineConfig({
  testDir: "./test/browser",
  timeout: 30_000,
  use: {
    baseURL: `http://127.0.0.1:${port}`
  },
  webServer: {
    command: `${python} -m http.server ${port}`,
    cwd: rootDir,
    reuseExistingServer: !process.env.CI,
    stdout: "ignore",
    stderr: "pipe"
  }
});
