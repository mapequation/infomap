#include "vendor/doctest.h"

#include "utils/Random.h"

#include <random>

namespace {

// Only the bounded-draw shape: no result_type, no min()/max(), no operator()().
// Compiling at the setEngine() call site proves this shape is sufficient.
struct BoundedStubEngine {
  unsigned int offset = 0;

  void seed(unsigned int) {}

  unsigned int randInt(unsigned int min, unsigned int /*max*/) { return min + offset; }
};

// Both shapes at once: randInt() returns a fixed sentinel while the bit
// generator only ever produces zero bits (which any distribution maps to the
// range minimum), so the two paths cannot agree on randInt(0, 10).
struct DualShapeEngine {
  using result_type = infomap::RandGen::result_type;

  void seed(unsigned int) {}

  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return infomap::RandGen::max(); }

  result_type operator()() { return 0; }

  unsigned int randInt(unsigned int /*min*/, unsigned int /*max*/) { return 7; }
};

// A plain UniformRandomBitGenerator without randInt(): a seeded mt19937, so
// the fallback mapping can be compared against std::uniform_int_distribution
// over the same generator state.
struct BitsOnlyEngine {
  using result_type = infomap::RandGen::result_type;

  infomap::RandGen gen;

  explicit BitsOnlyEngine(unsigned int seedValue) : gen(seedValue) {}

  void seed(unsigned int seedValue) { gen.seed(seedValue); }

  static constexpr result_type min() { return infomap::RandGen::min(); }
  static constexpr result_type max() { return infomap::RandGen::max(); }

  result_type operator()() { return gen(); }
};

TEST_CASE("A bounded-draw engine supplies randInt directly [fast][core][rng]")
{
  infomap::Random rand;
  rand.setEngine(BoundedStubEngine { 3 });

  // min + offset proves the engine's own mapping is used verbatim.
  CHECK(rand.randInt(10, 20) == 13);
  CHECK(rand.randInt(100, 200) == 103);
}

TEST_CASE("The bounded draw wins over bit generation when an engine has both [fast][core][rng]")
{
  infomap::Random rand;
  rand.setEngine(DualShapeEngine {});

  // The bit path would map all-zero bits to the range minimum (0); the
  // engine's own bounded draw returns the sentinel.
  CHECK(rand.randInt(0, 10) == 7);
}

TEST_CASE("A bits-only engine is mapped through the standard distribution [fast][core][rng]")
{
  infomap::Random rand;
  rand.setEngine(BitsOnlyEngine(42));

  // Regression lock: the fallback must keep matching what
  // std::uniform_int_distribution produces over the same generator state.
  infomap::RandGen reference(42);
  infomap::StdUniformUIntDistribution dist;
  for (int i = 0; i < 8; ++i) {
    const auto expected = dist(reference, infomap::StdUniformUIntDistribution::param_type(0u, 100u));
    CHECK(rand.randInt(0, 100) == expected);
  }
}

} // namespace
