#!/usr/bin/env node
"use strict";

const fsPromises = require("fs").promises;
const gitRawCommits = require("git-raw-commits");
const conventionalCommitsParser = require("conventional-commits-parser");

function getCommits(from = "", to = "HEAD") {
  const commits = [];

  const gitOpts = { from, to };

  return new Promise((resolve, reject) =>
    gitRawCommits(gitOpts)
      .pipe(conventionalCommitsParser())
      .on("data", (data) => commits.push(data))
      .on("finish", () => resolve(commits))
      .on("error", reject));
}

function getCommitWriter(filename) {
  return commits => fsPromises.writeFile(filename, JSON.stringify(commits, null, 2));
}

getCommits("2d94e92")
  .then(getCommitWriter("interfaces/js/src/commits.json"));

module.exports = { getCommits, getCommitWriter };
