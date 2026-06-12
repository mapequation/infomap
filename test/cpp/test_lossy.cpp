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

TEST_CASE("Lossy: lambda -> infinity reproduces the standard two-level map equation [fast][core][lossy]")
{
  InfomapWrapper plain(defaultFlags("--two-level"));
  plain.readInputData(networkFixturePath("lossy_benchmark.net"));
  plain.run();

  InfomapWrapper lossy(defaultFlags("--lossy --lambda 1000000"));
  lossy.readInputData(networkFixturePath("lossy_benchmark.net"));
  lossy.run();

  CHECK(lossy.codelength() == doctest::Approx(plain.codelength()).epsilon(1e-9));
  CHECK(canonicalPartition(lossy.getModules(1, false)) == canonicalPartition(plain.getModules(1, false)));
}

TEST_CASE("Lossy: paper benchmark partition gives the published code lengths at lambda 2 [fast][core][lossy]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2 --no-infomap --cluster-data " + clusterFixturePath("lossy_benchmark.clu")));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  // J = L_lossy + lambda * D
  CHECK(im.codelength() == doctest::Approx(2.730915964229051).epsilon(1e-9));
  CHECK(im.getLossyRate() == doctest::Approx(2.4232236565367433).epsilon(1e-9));
  CHECK(im.getLossyDistortion() == doctest::Approx(0.15384615384615385).epsilon(1e-9));
  // Markov entropy rate floor
  CHECK(im.getEntropyRate() == doctest::Approx(1.9005561812175085).epsilon(1e-9));

  // Identity: L_lossy = L_full - sum of noise losses (only the chain is noise)
  InfomapWrapper plain(defaultFlags("--two-level --no-infomap --cluster-data " + clusterFixturePath("lossy_benchmark.clu")));
  plain.readInputData(networkFixturePath("lossy_benchmark.net"));
  plain.run();
  CHECK(plain.codelength() == doctest::Approx(2.818018368324836).epsilon(1e-9));
  CHECK(plain.codelength() - im.getLossyRate() == doctest::Approx(0.3947947117880925).epsilon(1e-9));
}

TEST_CASE("Lossy: direct optimization at lambda 2 finds cliques standard and chain as one noise module [fast][core][lossy]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  checkCanonicalPartition(im, { { 1, 2, 3, 4, 5 }, { 6, 7, 8, 9, 10 }, { 11, 12, 13, 14, 15 } });
  CHECK(im.codelength() == doctest::Approx(2.730915964229051).epsilon(1e-9));
  CHECK(im.getLossyRate() == doctest::Approx(2.4232236565367433).epsilon(1e-9));
  CHECK(im.getLossyDistortion() == doctest::Approx(0.15384615384615385).epsilon(1e-9));
}

TEST_CASE("Lossy: lambda -> 0 lumps everything into one noise module [fast][core][lossy]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 0.001"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  CHECK(im.numTopModules() == 1);
  CHECK(im.getLossyRate() == doctest::Approx(0.0).epsilon(1e-9));
  CHECK(im.getLossyDistortion() == doctest::Approx(1.9005561812175085).epsilon(1e-9));
}

TEST_CASE("Lossy: reported codelength equals rate plus lambda times distortion [fast][core][lossy]")
{
  // Regression guard: the tree codelength is summed per module via calcCodelength,
  // which must not subtract a correction for the root index level. On this network
  // that root term is nonzero at lambda 1.5 (it cancels by coincidence at lambda 2).
  for (const auto* lambdaFlag : { "--lossy --lambda 1.5", "--lossy --lambda 2" }) {
    InfomapWrapper im(defaultFlags(lambdaFlag));
    im.readInputData(networkFixturePath("lossy_benchmark.net"));
    im.run();
    const double lambda = im.lossyLambda;
    CHECK(im.codelength() == doctest::Approx(im.getLossyRate() + lambda * im.getLossyDistortion()).epsilon(1e-9));
  }
}

TEST_CASE("Lossy: rerun on the same instance preserves partition and codelength [fast][core][lossy][lifecycle]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  const auto firstCodelength = im.codelength();
  const auto firstRate = im.getLossyRate();
  const auto firstPartition = canonicalPartition(im.getModules(1, false));

  im.run();
  CHECK(im.codelength() == doctest::Approx(firstCodelength).epsilon(1e-9));
  CHECK(im.getLossyRate() == doctest::Approx(firstRate).epsilon(1e-9));
  CHECK(canonicalPartition(im.getModules(1, false)) == firstPartition);
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_FEATURE_LOSSY_MAP_EQUATION
