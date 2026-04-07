import js from "@eslint/js";
import globals from "globals";
import tseslint from "typescript-eslint";

export default tseslint.config(
  {
    ignores: [
      "interfaces/js/generated/*.json",
      "interfaces/js/test/package/**",
      "dist/**",
      "build/**"
    ]
  },
  js.configs.recommended,
  ...tseslint.configs.recommendedTypeChecked,
  {
    files: ["interfaces/js/**/*.{ts,mts,cts}"],
    languageOptions: {
      parserOptions: {
        project: ["./tsconfig.json"],
        tsconfigRootDir: import.meta.dirname
      },
      globals: {
        ...globals.browser,
        ...globals.node
      }
    },
    rules: {
      "@typescript-eslint/consistent-type-imports": "error",
      "@typescript-eslint/no-floating-promises": "error"
    }
  },
  {
    files: [
      "interfaces/js/test/browser/**/*.ts",
      "interfaces/js/playwright.config.ts",
      "interfaces/js/vitest.config.ts"
    ],
    languageOptions: {
      globals: {
        ...globals.node
      }
    }
  }
);
