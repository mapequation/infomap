const path = require("path");
const DefinePlugin = require("webpack").DefinePlugin;
const getCommits = require("./get-commits.js");
const getParameters = require("./get-parameters.js");
const version = require("../../package.json").version;

const webpackConfig = async () => {
  const v1beta56hash = "63de1fea1c05cf23bf469cf07e6c8b387b0cb520";
  const commits = await getCommits(v1beta56hash);
  const { parameters } = await getParameters("./Infomap");

  return {
    mode: "production",
    entry: "./interfaces/js/src/index.js",
    devtool: "inline-source-map",
    devServer: {
      contentBase: "../../dist",
    },
    output: {
      filename: "index.js",
      path: path.resolve(__dirname, "../../dist"),
      library: "infomap",
      libraryTarget: "umd",
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
          test: /\.js$/,
          exclude: /(node_modules|utils|worker)/,
          use: {
            loader: "babel-loader",
            options: {
              presets: ["@babel/preset-env"],
              plugins: ["@babel/plugin-proposal-class-properties"],
            },
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

module.exports = webpackConfig;
