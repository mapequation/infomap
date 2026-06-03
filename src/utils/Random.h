/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef RANDOM_H_
#define RANDOM_H_

#include <cstdint>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

namespace infomap {
using RandGen = std::mt19937;
using StdUniformUIntDistribution = std::uniform_int_distribution<unsigned int>;

class PortableUniformUIntDistribution {
public:
  struct param_type {
    param_type(unsigned int minValue, unsigned int maxValue) : min(minValue), max(maxValue) {}

    unsigned int min;
    unsigned int max;
  };

  // Portable unbiased bounded integer mapping, independent of std::uniform_int_distribution internals.
  unsigned int operator()(RandGen& randGen, const param_type& params)
  {
    const auto min = params.min;
    const auto max = params.max;
    const std::uint64_t range = static_cast<std::uint64_t>(max) - min + 1u;
    const std::uint64_t generatorRange = static_cast<std::uint64_t>(RandGen::max()) - RandGen::min() + 1u;
    const std::uint64_t limit = generatorRange - (generatorRange % range);

    std::uint64_t value = 0;
    do {
      value = static_cast<std::uint64_t>(randGen()) - RandGen::min();
    } while (value >= limit);

    return min + static_cast<unsigned int>(value % range);
  }
};

template <typename UniformUIntDistribution>
class BasicRandom {
  RandGen m_randGen;
  UniformUIntDistribution m_uniform;

public:
  BasicRandom(unsigned int seed = 123) : m_randGen(seed) {}

  void seed(unsigned int seedValue)
  {
    m_randGen.seed(seedValue);
  }

  unsigned int randInt(unsigned int min, unsigned int max)
  {
    return m_uniform(m_randGen, typename UniformUIntDistribution::param_type(min, max));
  }

  /**
   * Get a random permutation of indices of the size of the input vector
   */
  void getRandomizedIndexVector(std::vector<unsigned int>& randomOrder)
  {
    unsigned int size = randomOrder.size();
    std::iota(randomOrder.begin(), randomOrder.end(), 0u);
    for (unsigned int i = 0; i < size; ++i)
      std::swap(randomOrder[i], randomOrder[i + randInt(0, size - i - 1)]);
  }
};

using StdRandom = BasicRandom<StdUniformUIntDistribution>;
using PortableRandom = BasicRandom<PortableUniformUIntDistribution>;
using Random = StdRandom;
} // namespace infomap

#endif // RANDOM_H_
