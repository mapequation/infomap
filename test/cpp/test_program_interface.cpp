#include "vendor/doctest.h"

#include "io/ProgramInterface.h"

#include <string>
#include <vector>

namespace {

TEST_CASE("ProgramInterface parses bool, incremental and positional arguments [fast][core][parser][cli]")
{
  bool toggle = false;
  bool feature = true;
  unsigned int verbosity = 0;
  unsigned int count = 0;
  std::string input;
  std::vector<std::string> extras;

  infomap::ProgramInterface api("Test", "Test parser", "1.0");
  api.addOptionArgument(toggle, 't', "toggle", "Toggle on", "Core");
  api.addOptionArgument(feature, "feature", "Feature flag", "Core");
  api.addIncrementalOptionArgument(verbosity, 'v', "verbose", "Verbose", "Core");
  api.addOptionArgument(count, 'c', "count", "Count", infomap::ArgType::integer, "Core");
  api.addNonOptionArgument(input, "input", "Input file", "Core");
  api.addOptionalNonOptionArguments(extras, "extras", "Extra files", "Core");

  api.parseArgs("-tvv -c7 --no-feature primary.net extra-a extra-b");

  CHECK(toggle);
  CHECK_FALSE(feature);
  CHECK(verbosity == 2u);
  CHECK(count == 7u);
  CHECK(input == "primary.net");
  CHECK(extras == std::vector<std::string> { "extra-a", "extra-b" });

  const auto used = api.getUsedOptionArguments();
  REQUIRE(used.size() == 4);
  CHECK(used[0].longName == "toggle");
  CHECK(used[1].longName == "feature");
  CHECK(used[1].negated);
  CHECK(used[2].longName == "verbose");
  CHECK(used[3].longName == "count");
}

TEST_CASE("ProgramInterface parses long options with separate values [fast][core][parser][cli]")
{
  unsigned long seed = 0;
  double rate = 0.0;

  infomap::ProgramInterface api("Test", "Test parser", "1.0");
  api.addOptionArgument(seed, 's', "seed", "Seed", infomap::ArgType::integer, "Core");
  api.addOptionArgument(rate, '\0', "rate", "Rate", infomap::ArgType::number, "Core");

  api.parseArgs("--seed 123 --rate 0.25");

  CHECK(seed == 123ul);
  CHECK(rate == doctest::Approx(0.25));
}

} // namespace
