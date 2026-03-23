import {getRawCommitsStream} from "git-raw-commits";
import { CommitParser, parseCommitsStream } from "conventional-commits-parser";
import { Readable } from 'stream'

export default function getCommits(from = "", to = "HEAD") {
  const commits = [];

  const gitOpts = { from, to, format: "%B%n-date-%n%aI" };

  return new Promise((resolve, reject) =>
    getRawCommitsStream(gitOpts)
      .pipe(parseCommitsStream(new CommitParser()))
      .on("data", (data) => commits.push(data))
      .on("finish", () => resolve(commits))
      .on("error", reject)
  );
}
