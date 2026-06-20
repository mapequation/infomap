// Tests for the soft --preferred-number-of-levels depth preference (issue #308).
//
// The preference is asymmetric: steering to a SHALLOWER depth than the
// unconstrained solution is reliable, while DEEPER is best-effort. It must be a
// complete no-op when the option is unset, when its strength is zero, and under
// --two-level. See src/core/InfomapBase.cpp (prefLevelsBias and its two
// hierarchy-stage accept sites).

#include "TestUtils.h"

#include <cstdio>
#include <sstream>

using infomap::InfomapWrapper;

namespace {

// The unbalanced fixture resolves to a 3-level hierarchy at seed 123: a shallow
// branch of heavy triangles plus one deeper branch of nested triangles. Asserted
// here so the steering tests below have a known starting depth to steer away from.
constexpr unsigned int kUnconstrainedLevels = 3;

// The default strength (1.0) steers this 36-node fixture, the same value that
// steers the ~500k-node nets: the depth preference is an MDL prior expressed in
// real bits, so its strength is network-size independent (no per-network tuning).
constexpr double kSteeringStrength = 1.0;

std::string treeOf(const std::string& extraFlags)
{
  // writeTree() writes a .tree file and returns its PATH, not the contents, so
  // capture the actual tree by writing to a unique temp file and reading it back
  // (a fresh path per call avoids the overwrite guard on repeated comparisons).
  // Strip the leading "#" comment lines: they echo the command line and a timing
  // line, which differ across runs/flags even when the partition is identical, so
  // comparing the stripped body is the real byte-identity check.
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

  // Option entirely absent — must reproduce the baseline tree exactly.
  CHECK(treeOf("") == baseline);

  // Strength set but level unset (level 0 = off) — also a no-op.
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

  // The depth preference must not perturb a two-level run, regardless of target.
  CHECK(treeOf("--two-level --preferred-number-of-levels 2 --preferred-number-of-levels-strength 5") == twoLevelBaseline);
  CHECK(treeOf("--two-level --preferred-number-of-levels 5 --preferred-number-of-levels-strength 5") == twoLevelBaseline);
}
