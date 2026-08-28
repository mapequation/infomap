#include "vendor/doctest.h"

#include "utils/infomath.h"

#include <cmath>
#include <vector>

using infomap::infomath::linlog;
using infomap::infomath::tsallisEntropyUniform;

namespace {

const std::vector<double> kValues = { 1, 2, 3, 5, 10, 64, 1000, 1e5 };

// Central difference is not usable across q = 1: the corrections are held
// constant above it, so the two sides are different expressions and a symmetric
// stencil would average them. One-sided slopes are what the smoothness claim is
// about.
double slopeFromLeft(double k, double q, double h = 1e-6)
{
  return (linlog(k, q) - linlog(k, q - h)) / h;
}

double slopeFromRight(double k, double q, double h = 1e-6)
{
  return (linlog(k, q + h) - linlog(k, q)) / h;
}

} // namespace

TEST_CASE("linlog reproduces its endpoints exactly [fast][core][utils]")
{
  for (double k : kValues) {
    CHECK(linlog(k, 0) == doctest::Approx(k).epsilon(1e-12));
    CHECK(linlog(k, 1) == doctest::Approx(std::log2(k)).epsilon(1e-12));
  }
}

TEST_CASE("linlog is smooth in q where the corrections stop varying [fast][core][utils]")
{
  // The corrections are frozen for q > 1, so the interpolation joins that branch
  // smoothly only if the shape variable has zero slope at q = 1. Without it the
  // derivative jumps and a damping sweep through 1 shows a kink.
  for (double k : { 2.0, 64.0, 1000.0 }) {
    CHECK(linlog(k, 1) == doctest::Approx(linlog(k, 1 + 1e-9)).epsilon(1e-9));
    CHECK(slopeFromLeft(k, 1) == doctest::Approx(slopeFromRight(k, 1)).epsilon(1e-4));
  }
}

TEST_CASE("linlog keeps the monotonicity the flow model relies on [fast][core][utils]")
{
  // Local Markov time is a ratio of scales, so the scale has to grow with the
  // local flow and damping has to shrink the spread rather than invert it.
  for (double q = 0; q <= 1.0001; q += 0.1) {
    for (std::size_t i = 1; i < kValues.size(); ++i) {
      CHECK(linlog(kValues[i], q) > linlog(kValues[i - 1], q));
    }
  }

  for (double k : { 2.0, 10.0, 1000.0 }) {
    double previous = linlog(k, 0);
    for (double q = 0.1; q <= 1.0001; q += 0.1) {
      const double current = linlog(k, q);
      CHECK(current <= previous + 1e-12);
      previous = current;
    }
  }
}

TEST_CASE("linlog leaves the Tsallis entropy uncorrected above q = 1 [fast][core][utils]")
{
  for (double k : kValues) {
    for (double q : { 1.5, 2.0, 4.0 }) {
      CHECK(linlog(k, q) == doctest::Approx(tsallisEntropyUniform(k, q)).epsilon(1e-12));
    }
  }
}
