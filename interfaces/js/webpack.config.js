const path = require("path");

module.exports = {
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
          },
        },
      },
    ],
  },
};
