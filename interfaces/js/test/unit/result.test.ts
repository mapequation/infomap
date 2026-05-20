import { describe, expect, test } from "vitest";
import {
  getResultFiles,
  getResultMetadata,
  resultFormats,
  type ResultFormat,
} from "../../src/result";
import outputFormatsManifest from "../../generated/output-formats.json";
import type { Result } from "../../src";

describe("result helpers", () => {
  test("derives metadata from JSON output", () => {
    const result: Result = {
      json: {
        version: "2.10.1",
        args: "--json",
        startedAt: "now",
        completedIn: 1,
        codelength: 2.01141,
        numLevels: 3,
        numTopModules: 2,
        relativeCodelengthSavings: 0.1,
        directed: false,
        flowModel: "undirected",
        higherOrder: false,
        nodes: [],
        modules: [],
      },
    };

    expect(getResultMetadata(result)).toEqual({
      codelength: 2.01141,
      numLevels: 3,
    });
  });

  test("prefers state JSON metadata over physical JSON metadata", () => {
    const base = {
      version: "2.10.1",
      args: "--json",
      startedAt: "now",
      completedIn: 1,
      numTopModules: 2,
      relativeCodelengthSavings: 0.1,
      directed: false,
      flowModel: "undirected",
      higherOrder: false,
      nodes: [],
      modules: [],
    };

    expect(
      getResultMetadata({
        json: { ...base, codelength: 1, numLevels: 1 },
        json_states: { ...base, codelength: 2, numLevels: 4 },
      }),
    ).toEqual({ codelength: 2, numLevels: 4 });
  });

  test("falls back to text headers when JSON is missing", () => {
    expect(
      getResultMetadata({
        clu: [
          "# v2.9.2",
          "# partitioned into 3 levels with 5 top modules",
          "# codelength 3.46227 bits",
          "# node_id module flow",
          "1 1 0.5",
        ].join("\n"),
      }),
    ).toEqual({ codelength: 3.46227, numLevels: 3 });
  });

  test("falls back to tree depth when text headers do not include levels", () => {
    expect(
      getResultMetadata({
        tree: '1:2:1 0.2 "node" 7\n',
      }),
    ).toEqual({ codelength: null, numLevels: 3 });
  });

  test("derives downloadable result files", () => {
    const files = getResultFiles(
      {
        clu: "1 1\n",
        ftree: '1:1 0.1 "node" 1\n',
        json: {
          version: "2.10.1",
          args: "--json",
          startedAt: "now",
          completedIn: 1,
          codelength: 2.01141,
          numLevels: 1,
          numTopModules: 1,
          relativeCodelengthSavings: 0,
          directed: false,
          flowModel: "undirected",
          higherOrder: false,
          nodes: [],
          modules: [],
        },
        states: "*States\n",
        flow_as_physical: "1 2 0.5\n",
      },
      "network",
    );

    expect(files.map((file) => file.filename)).toEqual([
      "network.clu",
      "network.ftree",
      "network_states.net",
      "network_states_as_physical_flow.net",
      "network.json",
    ]);
    expect(files.find((file) => file.key === "json")?.content).toContain(
      '\n  "codelength": 2.01141,\n',
    );
    expect(files.find((file) => file.key === "json")?.mimeType).toBe(
      "application/json;charset=utf-8",
    );
  });

  test("keeps known result format metadata available", () => {
    const byKey = new Map(
      resultFormats.map((format: ResultFormat) => [format.key, format]),
    );

    expect(byKey.get("clu")?.extension).toBe("clu");
    expect(byKey.get("json_states")?.suffix).toBe("_states");
    expect(byKey.get("flow_as_physical")?.isStates).toBe(true);
  });

  test("uses generated output format metadata in result order", () => {
    const generatedFormats = new Map(
      outputFormatsManifest.formats
        .flatMap((format) => format.files)
        .map((format) => [format.key, format]),
    );

    expect(resultFormats.map((format) => format.key)).toEqual(
      outputFormatsManifest.resultOrder,
    );
    expect(resultFormats).toEqual(
      outputFormatsManifest.resultOrder.map((key) => generatedFormats.get(key)),
    );
  });
});
