import path from "path";
import { fileURLToPath } from "url";
import webpack from "webpack";
import getCommits from "./get-commits.js";
import getParameters from "./get-parameters.js";

const DefinePlugin = webpack.DefinePlugin;
import { readFile } from "fs/promises";

export default async () => {
  const pkg = JSON.parse(await readFile(new URL("../../package.json", import.meta.url), "utf8"));
  const version = pkg.version;
  // __dirname is not defined in ESM; emulate it
  const __filename = fileURLToPath(import.meta.url);
  const __dirname = path.dirname(__filename);
  const v1beta56hash = "63de1fea1c05cf23bf469cf07e6c8b387b0cb520";
  const commits = await getCommits(v1beta56hash);
  const { parameters } = await getParameters("./Infomap");

  console.log(`Building infomap.js v${version} with ${parameters.length} parameters and${commits.length} commits since v1beta56 (${v1beta56hash})...`);

  return {
    mode: "production",
    entry: {
      index: "./interfaces/js/src/index.ts",
    },
    output: {
      filename: "[name].js",
      path: path.resolve(__dirname, "../../"),
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
        CHANGELOG: JSON.stringify(commits, null, 2),
        VERSION: JSON.stringify(version),
        PARAMETERS: JSON.stringify(parameters, null, 2),
      }),
    ],
  };
};

