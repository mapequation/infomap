const path = require("path");
const DefinePlugin = require("webpack").DefinePlugin;
const getCommits = require("../../utils/get-commits.js").getCommits;
const getParameters = require("../../utils/get-parameters.js").getParameters;
const version = require("../../package.json").version;


const webpackConfig = async () => {
  const commits = await getCommits("2d94e92");
  const { parameters } = JSON.parse(await getParameters("./Infomap"));

  return {
    mode: "production",
    entry: "./interfaces/js/src/index.js",
    devtool: "inline-source-map",
    devServer: {
      contentBase: "./interfaces/js/dist",
    },
    output: {
      filename: "index.js",
      path: path.resolve(__dirname, "dist"),
      library: "infomap",
      libraryTarget: "umd",
    },
    module: {
      rules: [
        {
          test: /\.js$/,
          exclude: /(node_modules)/,
          use: {
            loader: "babel-loader",
            options: {
              presets: ["@babel/preset-env"],
              plugins: ["@babel/plugin-proposal-class-properties"]
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
      })
    ]
  }
};

module.exports = webpackConfig
