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
  // The noise credit is derived against the standard map equation: it hands back
  // sum_leaf plogp(f) at coefficient 1, which is the rate scoreStackBase charges it
  // at, while L* charges nrLeafCodebookRate >= 1. See #1011 / F38.
  throws("--lossy --non-redundant");
}

TEST_CASE("Lossy: rejects unsupported input detected after parsing [fast][core][lossy][columnar-contract]")
{
  // Config validation runs before the input file is parsed; state input flips
  // haveMemory() afterwards, so the optimizer dispatch must re-validate.
  InfomapWrapper im(defaultFlags("--lossy"));
  im.readInputData(networkFixturePath("states.net"));
  CHECK_THROWS_AS(im.run(), std::runtime_error);
}

TEST_CASE("Lossy: the undirected-flow guard is reachable from the input, not only from a flag [fast][core][lossy][config]")
{
  // #1004: adaptDefaults rejects --directed and an explicitly set --flow-model, so the
  // late guard's flow-model clause only ever sees a model that adaptDefaults did not get
  // to look at. The four subcases below are the ways that happens: the network read; a
  // plain assignment on a Config, which leaves flowModelIsSet false; and either config
  // setter called on a constructed instance, where adaptDefaults no longer runs. A
  // directed edge list is not one of them: *Arcs under an undirected flow model is parsed
  // as undirected and the model stays undirected. Every case is checked on the message
  // rather than on the exception type -- the memory/multilayer clause above throws a
  // std::runtime_error too, and it is the clause that fires for ordinary multilayer
  // input.
  auto messageOf = [](auto&& run) {
    try {
      run();
    } catch (const std::runtime_error& e) {
      return std::string(e.what());
    }
    return std::string("<no exception>");
  };

  SUBCASE("multilayer expansion whose state map is identity")
  {
    // Expansion marks the network as directed input, and configureNetworkMode() then
    // switches the flow model. Ordinary multilayer input throws one clause earlier,
    // because expansion gives state ids that differ from the physical ids; this fixture's
    // coincide, so haveMemoryInput() stays false, neither setStateInput() nor
    // setMultilayerInput() runs, and the flow-model clause is the only one left.
    InfomapWrapper im(defaultFlags("--lossy"));
    im.readInputData(networkFixturePath("intra_identity_states.net"));
    CHECK(messageOf([&] { im.run(); }) == "--lossy requires undirected flow");
  }

  SUBCASE("embedder assigning flowModel on a Config before adaptDefaults")
  {
    // A plain assignment leaves flowModelIsSet false, so adaptDefaults' clause (which
    // tests directed || (flowModelIsSet && ...)) does not see it, and it arrives here.
    Config config;
    config.lossy = true;
    config.flowModel = FlowModel::directed;
    CHECK_NOTHROW(config.adaptDefaults());
    CHECK(messageOf([&] {
            InfomapWrapper im { config };
            (void)im;
          })
          == "--lossy requires undirected flow");
  }

  SUBCASE("embedder calling setDirected on a live instance")
  {
    // InfomapConfig::setDirected writes flowModel too, and adaptDefaults ran once while
    // the flag string was parsed and never runs again, so the parse-time clause cannot
    // see this. The next initOptimizer() -- here from run() via initNetwork() -- catches
    // it instead.
    InfomapWrapper im(defaultFlags("--lossy"));
    im.readInputData(networkFixturePath("twotriangles_flow.net"));
    im.setDirected(true);
    CHECK(messageOf([&] { im.run(); }) == "--lossy requires undirected flow");
  }

  SUBCASE("embedder calling setFlowModel on a live instance")
  {
    // setFlowModel does set flowModelIsSet, but that only matters to adaptDefaults, which
    // has already run. On a constructed instance it is the same late guard that catches
    // it.
    InfomapWrapper im(defaultFlags("--lossy"));
    im.readInputData(networkFixturePath("twotriangles_flow.net"));
    im.setFlowModel(FlowModel::directed);
    CHECK(messageOf([&] { im.run(); }) == "--lossy requires undirected flow");
  }
}

TEST_CASE("Lossy: input constraints are owned by the optimizer dispatch, not by adaptDefaults [fast][core][lossy][config]")
{
  // #1004: adaptDefaults used to reject stateInput/multilayerInput/additionalInput, which
  // no CLI option can set at parse time -- configureNetworkMode() writes the first two
  // when the network is read, and additionalInput is library-only. The check was
  // therefore only reachable by a library caller mutating the struct, and even there the
  // same rejection arrives one stage later from initOptimizer(). The second CHECK of each
  // pair is what makes the deletion safe rather than merely quiet: it pins that the late
  // guard still covers every field the parse-time one did. Brace-initialised inside the
  // macro on purpose: InfomapWrapper(config) risks parsing as a declaration.
  SUBCASE("state input")
  {
    Config config;
    config.lossy = true;
    config.setStateInput();
    CHECK_NOTHROW(config.adaptDefaults());
    CHECK_THROWS_AS(InfomapWrapper { config }, std::runtime_error);
  }

  SUBCASE("multilayer input")
  {
    Config config;
    config.lossy = true;
    config.setMultilayerInput();
    CHECK_NOTHROW(config.adaptDefaults());
    CHECK_THROWS_AS(InfomapWrapper { config }, std::runtime_error);
  }

  SUBCASE("additional input")
  {
    Config config;
    config.lossy = true;
    config.additionalInput.push_back("extra.net");
    CHECK_NOTHROW(config.adaptDefaults());
    CHECK_THROWS_AS(InfomapWrapper { config }, std::runtime_error);
  }

  SUBCASE("meta data stays a parse-time rejection")
  {
    // Unlike the three above, --meta-data is a flag, so this one is decidable before the
    // input is read and keeps failing early.
    Config config;
    config.lossy = true;
    config.metaDataFile = "foo.txt";
    CHECK_THROWS_AS(config.adaptDefaults(), std::runtime_error);
  }
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

TEST_CASE("Lossy: paper benchmark partition gives the published code lengths at lambda 2 [fast][core][lossy][columnar-contract]")
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

TEST_CASE("Lossy: direct optimization at lambda 2 finds cliques standard and chain as one noise module [fast][core][lossy][columnar-contract]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  checkCanonicalPartition(im, { { 1, 2, 3, 4, 5 }, { 6, 7, 8, 9, 10 }, { 11, 12, 13, 14, 15 } });
  CHECK(im.codelength() == doctest::Approx(2.730915964229051).epsilon(1e-9));
  CHECK(im.getLossyRate() == doctest::Approx(2.4232236565367433).epsilon(1e-9));
  CHECK(im.getLossyDistortion() == doctest::Approx(0.15384615384615385).epsilon(1e-9));
}

TEST_CASE("Lossy: lambda -> 0 lumps everything into one noise module [fast][core][lossy][columnar-contract]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 0.001"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  CHECK(im.numTopModules() == 1);
  CHECK(im.getLossyRate() == doctest::Approx(0.0).epsilon(1e-9));
  CHECK(im.getLossyDistortion() == doctest::Approx(1.9005561812175085).epsilon(1e-9));
}

TEST_CASE("Lossy: reported codelength equals rate plus lambda times distortion [fast][core][lossy][columnar-contract]")
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

TEST_CASE("Lossy: rerun on the same instance preserves partition and codelength [fast][core][lossy][lifecycle][columnar-contract]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();
  const auto firstCodelength = im.codelength();
  const auto firstRate = im.getLossyRate();
  const auto firstOneLevel = im.getReferenceOneLevelCodelength();
  const auto firstPartition = canonicalPartition(im.getModules(1, false));

  im.run();
  CHECK(im.codelength() == doctest::Approx(firstCodelength).epsilon(1e-9));
  CHECK(im.getLossyRate() == doctest::Approx(firstRate).epsilon(1e-9));
  // L_1 must stay the lossless one-level reference across reruns, not drift to a
  // partition-dependent value if calcCodelength is called again on the root.
  CHECK(im.getReferenceOneLevelCodelength() == doctest::Approx(firstOneLevel).epsilon(1e-9));
  CHECK(canonicalPartition(im.getModules(1, false)) == firstPartition);
}

TEST_CASE("Lossy: tree output header reports lambda, rate and distortion [fast][core][lossy][output][columnar-contract]")
{
  InfomapWrapper im(defaultFlags("--lossy --lambda 2"));
  im.readInputData(networkFixturePath("lossy_benchmark.net"));
  im.run();

  const std::string treePath = "/tmp/infomap_lossy_header_test.tree";
  std::remove(treePath.c_str());
  im.writeTree(treePath);
  const auto content = readTextFile(treePath);

  CHECK(content.find("# lossy lambda 2") != std::string::npos);
  CHECK(content.find("rate 2.42322") != std::string::npos);
  CHECK(content.find("distortion 0.153846") != std::string::npos);
  std::remove(treePath.c_str());
}

TEST_CASE("Lossy: reciprocal link listing gives identical results to single listing [fast][core][lossy][columnar-contract]")
{
  // h_alpha is the transition entropy over distinct neighbours, so an undirected
  // edge written in both directions (as the demo app and many .net files do) must
  // not be double-counted. Single- and reciprocal-listed inputs must agree exactly.
  InfomapWrapper single(defaultFlags("--lossy --lambda 2"));
  single.readInputData(networkFixturePath("lossy_benchmark.net"));
  single.run();

  InfomapWrapper reciprocal(defaultFlags("--lossy --lambda 2"));
  reciprocal.readInputData(networkFixturePath("lossy_benchmark_reciprocal.net"));
  reciprocal.run();

  CHECK(reciprocal.codelength() == doctest::Approx(single.codelength()).epsilon(1e-9));
  CHECK(reciprocal.getLossyRate() == doctest::Approx(single.getLossyRate()).epsilon(1e-9));
  CHECK(reciprocal.getLossyDistortion() == doctest::Approx(single.getLossyDistortion()).epsilon(1e-9));
  CHECK(reciprocal.getReferenceOneLevelCodelength() == doctest::Approx(single.getReferenceOneLevelCodelength()).epsilon(1e-9));
  CHECK(canonicalPartition(reciprocal.getModules(1, false)) == canonicalPartition(single.getModules(1, false)));
}

TEST_CASE("Lossy: reported one-level reference is the lambda-independent lossless L1 [fast][core][lossy][columnar-contract]")
{
  // The one-level reference must be the lossless L1 = H(p_alpha) (paper sec 2.5),
  // not the lambda-dependent noise-corrected one-module objective.
  InfomapWrapper plain(defaultFlags("--two-level"));
  plain.readInputData(networkFixturePath("lossy_benchmark.net"));
  plain.run();
  const double L1 = plain.getOneLevelCodelength();

  double previous = -1.0;
  for (const auto* lambdaFlag : { "--lossy --lambda 0.5", "--lossy --lambda 1", "--lossy --lambda 2", "--lossy --lambda 10" }) {
    InfomapWrapper im(defaultFlags(lambdaFlag));
    im.readInputData(networkFixturePath("lossy_benchmark.net"));
    im.run();
    CHECK(im.getReferenceOneLevelCodelength() == doctest::Approx(L1).epsilon(1e-9));
    if (previous >= 0.0)
      CHECK(im.getReferenceOneLevelCodelength() == doctest::Approx(previous).epsilon(1e-9));
    previous = im.getReferenceOneLevelCodelength();
  }
}

TEST_CASE("Lossy: noiseTopModules reports which top modules are noise [fast][core][lossy][columnar-contract]")
{
  // At lambda 2 only the chain is noise; toward the standard map equation none are;
  // at lambda -> 0 the single all-network module is noise.
  InfomapWrapper mid(defaultFlags("--lossy --lambda 2"));
  mid.readInputData(networkFixturePath("lossy_benchmark.net"));
  mid.run();
  CHECK(mid.noiseTopModules().size() == 1);
  CHECK(mid.noiseTopModules().size() == static_cast<std::size_t>(mid.getLossyDistortion() > 0 ? 1 : 0));

  InfomapWrapper standard(defaultFlags("--lossy --lambda 10"));
  standard.readInputData(networkFixturePath("lossy_benchmark.net"));
  standard.run();
  CHECK(standard.noiseTopModules().empty());
  CHECK(standard.getLossyDistortion() == doctest::Approx(0.0).epsilon(1e-9));

  InfomapWrapper lump(defaultFlags("--lossy --lambda 0.001"));
  lump.readInputData(networkFixturePath("lossy_benchmark.net"));
  lump.run();
  CHECK(lump.numTopModules() == 1);
  CHECK(lump.noiseTopModules() == std::vector<unsigned int> { 1 });
}

} // namespace test
} // namespace infomap

#endif // INFOMAP_FEATURE_LOSSY_MAP_EQUATION
