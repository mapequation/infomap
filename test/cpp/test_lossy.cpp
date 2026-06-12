#include "TestUtils.h"
#include "io/Config.h"

#if INFOMAP_FEATURE_LOSSY_MAP_EQUATION

namespace infomap {
namespace test {

namespace {

Config lossyConfig(const std::string& extraFlags)
{
  return Config("input.net --silent --no-file-output " + extraFlags, true);
}

} // namespace

TEST_CASE("Lossy: --lossy parses, sets lambda and implies two-level [fast][core][lossy]")
{
  const auto conf = lossyConfig("--lossy --lambda 2");
  CHECK(conf.lossy);
  CHECK(conf.lossyLambda == doctest::Approx(2.0));
  CHECK(conf.twoLevel);
}

TEST_CASE("Lossy: default lambda is 1 [fast][core][lossy]")
{
  const auto conf = lossyConfig("--lossy");
  CHECK(conf.lossyLambda == doctest::Approx(1.0));
}

TEST_CASE("Lossy: rejects unsupported configurations [fast][core][lossy]")
{
  auto throws = [](const std::string& flags) {
    CHECK_THROWS_AS(lossyConfig(flags), std::runtime_error);
  };
  throws("--lossy --directed");
  throws("--lossy --flow-model directed");
  throws("--lossy --meta-data foo.txt");
  throws("--lossy --regularized");
  throws("--lossy --recorded-teleportation");
  throws("--lossy --variable-markov-time");
  throws("--lossy --markov-time 2");
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_FEATURE_LOSSY_MAP_EQUATION
