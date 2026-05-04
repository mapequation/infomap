#include "vendor/doctest.h"

#include "io/Config.h"

namespace {

using infomap::Config;
TEST_CASE("Config accepts no-file-output without an output directory [fast][core][config][cli]")
{
  CHECK_NOTHROW(Config("input.net --silent --no-file-output", true));

  const Config config("input.net --silent --no-file-output", true);
  CHECK(config.noFileOutput);
  CHECK(config.outDirectory.empty());
  CHECK(config.networkFile == "input.net");
}

TEST_CASE("Config parses flow model selection and output formats [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --flow-model precomputed --output json,tree", true);
  CHECK(infomap::flowModelToString(config.flowModel) == std::string("precomputed"));
  CHECK(config.printJson);
  CHECK(config.printTree);
}

TEST_CASE("Config parses solution landscape tracking options [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --solution-landscape-tracking --solution-landscape-stop-after 3", true);
  CHECK(config.solutionLandscapeTracking);
  CHECK(config.solutionLandscapeStopAfter == 3);
}

TEST_CASE("Config allows solution landscape tracking with inner parallelization [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --solution-landscape-tracking --inner-parallelization", true);
  CHECK(config.solutionLandscapeTracking);
  CHECK(config.innerParallelization);
}

TEST_CASE("Config treats directed shorthand as directed flow model [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --directed", true);
  CHECK(infomap::flowModelToString(config.flowModel) == std::string("directed"));
}

TEST_CASE("Config rejects unknown flow models and output formats [fast][core][config][cli]")
{
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --flow-model unknown", true), std::runtime_error);
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --output tree,unknown", true), std::runtime_error);
}

TEST_CASE("Config rejects invalid solution landscape option combinations [fast][core][config][cli]")
{
  CHECK_THROWS_WITH_AS(
      Config("input.net --silent --no-file-output --solution-landscape-stop-after 2", true),
      "The --solution-landscape-stop-after option requires --solution-landscape-tracking.",
      std::runtime_error);
}

} // namespace
