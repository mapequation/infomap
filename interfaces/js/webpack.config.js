import path from "path";
import { fileURLToPath } from "url";
import webpack from "webpack";

const DefinePlugin = webpack.DefinePlugin;
import { readFile } from "fs/promises";

export default async () => {
  const pkg = JSON.parse(await readFile(new URL("../../package.json", import.meta.url), "utf8"));
  const version = pkg.version;
  // __dirname is not defined in ESM; emulate it
  const __filename = fileURLToPath(import.meta.url);
  const __dirname = path.dirname(__filename);
  const changelog = JSON.parse(
    await readFile(new URL("./generated/changelog.json", import.meta.url), "utf8")
  );
  const parameters = JSON.parse(
    await readFile(new URL("./generated/parameters.json", import.meta.url), "utf8")
  );

  console.log(`Building infomap.js v${version} with ${parameters.length} parameters and ${changelog.length} changelog entries...`);

  return {
    mode: "production",
    entry: {
      index: "./interfaces/js/src/index.ts",
    },
    output: {
      filename: "[name].js",
      path: path.resolve(__dirname, "../../dist/npm/package"),
      library: {
        name: "infomap",
        type: "umd",
      },
    },
    resolve: {
      extensions: [".ts", ".js", ".js.mem"],
    },
    module: {
      rules: [
        {
          test: /\.worker\.js$/,
          use: {
            loader: "raw-loader",
          },
        },
        {
          test: /\.mem$/,
          use: {
            loader: "arraybuffer-loader",
          },
        },
        {
          test: /\.ts$/,
          exclude: /(node_modules|utils|worker)/,
          use: {
            loader: "ts-loader",
          },
        },
      ],
    },
    plugins: [
      new DefinePlugin({
        CHANGELOG: JSON.stringify(changelog, null, 2),
        VERSION: JSON.stringify(version),
        PARAMETERS: JSON.stringify(parameters, null, 2),
      }),
    ],
  };
};
