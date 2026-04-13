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

TEST_CASE("Config parses preferred number of levels [fast][core][config][cli]")
{
  const Config config("input.net --silent --no-file-output --preferred-number-of-levels 2", true);
  CHECK(config.preferredNumberOfLevels == 2u);
}

TEST_CASE("Preferred number of levels option enforces a lower bound of two [fast][core][config][cli]")
{
  unsigned int preferredNumberOfLevels = 0;
  infomap::LowerBoundArgumentOption<unsigned int> option(
      preferredNumberOfLevels,
      '\0',
      "preferred-number-of-levels",
      "Prefer solutions close to this many levels.",
      "Algorithm",
      true,
      infomap::ArgType::integer,
      2u);

  CHECK(option.parse("2"));
  CHECK(preferredNumberOfLevels == 2u);
  CHECK_FALSE(option.parse("1"));
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
  CHECK_THROWS_AS(Config("input.net --silent --no-file-output --two-level --preferred-number-of-levels 2", true), std::runtime_error);
}

} // namespace
