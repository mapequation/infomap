import commits from "./commits.json";
import { version } from "../../../package.json";
import Infomap from "./worker.js";


export {
  Infomap as default,
  version as infomapVersion,
  commits as infomapChangelog
};
