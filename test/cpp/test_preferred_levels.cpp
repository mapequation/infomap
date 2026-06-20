// Tests for the soft --preferred-number-of-levels depth preference (issue #308).

#include "TestUtils.h"

#include <cstdio>
#include <sstream>

using infomap::InfomapWrapper;

namespace {

// Unconstrained depth of the fixture at seed 123 (the steering tests steer away from it).
constexpr unsigned int kUnconstrainedLevels = 3;

constexpr double kSteeringStrength = 1.0;

std::string treeOf(const std::string& extraFlags)
{
  // writeTree returns the output filename, not the tree; read the file back and
  // drop "#" header lines (arg echo + a nondeterministic timing line) before comparing.
  static int counter = 0;
  const std::string path = "preferred_levels_test_" + std::to_string(counter++) + ".tree";
  InfomapWrapper im(infomap::test::defaultFlags(extraFlags));
  infomap::test::readNetworkFixture(im, "unbalanced_hierarchy.net");
  im.run();
  im.writeTree(path);
  const std::string raw = infomap::test::readTextFile(path);
  std::remove(path.c_str());
  std::string body;
  std::istringstream in(raw);
  for (std::string line; std::getline(in, line);) {
    if (line.empty() || line[0] != '#') body += line + '\n';
  }
  return body;
}

unsigned int levelsOf(const std::string& extraFlags)
{
  InfomapWrapper im(infomap::test::defaultFlags(extraFlags));
  infomap::test::readNetworkFixture(im, "unbalanced_hierarchy.net");
  im.run();
  infomap::test::checkRunSanity(im);
  return im.numLevels();
}

} // namespace

TEST_CASE("Unbalanced fixture resolves to the expected unconstrained depth [fast][core][preferred-levels]")
{
  CHECK(levelsOf("") == kUnconstrainedLevels);
}

TEST_CASE("preferred-number-of-levels unset is a no-op (byte-identical tree) [fast][core][preferred-levels]")
{
  const auto baseline = treeOf("");
  CHECK(treeOf("") == baseline);
  CHECK(treeOf("--preferred-number-of-levels-strength 5") == baseline);
}

TEST_CASE("preferred-number-of-levels with zero strength is a no-op (byte-identical tree) [fast][core][preferred-levels]")
{
  const auto baseline = treeOf("");
  CHECK(treeOf("--preferred-number-of-levels 2 --preferred-number-of-levels-strength 0") == baseline);
}

TEST_CASE("Shallower preference reliably steers to the requested depth [fast][core][preferred-levels]")
{
  REQUIRE(levelsOf("") > 2); // there is depth to remove

  const std::string flags = "--preferred-number-of-levels 2 --preferred-number-of-levels-strength "
      + std::to_string(kSteeringStrength);
  CHECK(levelsOf(flags) == 2);
}

TEST_CASE("Shallower steering is deterministic for a fixed seed [fast][core][preferred-levels]")
{
  const std::string flags = "--preferred-number-of-levels 2 --preferred-number-of-levels-strength "
      + std::to_string(kSteeringStrength);
  CHECK(treeOf(flags) == treeOf(flags));
}

TEST_CASE("preferred-number-of-levels is a no-op under --two-level [fast][core][preferred-levels]")
{
  const auto twoLevelBaseline = treeOf("--two-level");
  CHECK(treeOf("--two-level --preferred-number-of-levels 2 --preferred-number-of-levels-strength 5") == twoLevelBaseline);
  CHECK(treeOf("--two-level --preferred-number-of-levels 5 --preferred-number-of-levels-strength 5") == twoLevelBaseline);
}
