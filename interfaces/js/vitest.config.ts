import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    include: ["interfaces/js/test/unit/**/*.test.ts"],
    setupFiles: ["interfaces/js/test/setup/unit.ts"]
  }
});
