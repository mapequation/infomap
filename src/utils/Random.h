/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef RANDOM_H_
#define RANDOM_H_

#include <cmath>
#include <vector>

#ifndef PYTHON
#include <random>
#include <utility>

namespace infomap {
typedef std::mt19937 RandGen;
typedef std::uniform_int_distribution<unsigned int> uniform_uint_dist;
typedef uniform_uint_dist::param_type uniform_param_t;

class Random {
  RandGen m_randGen;
  uniform_uint_dist uniform;

public:
  Random(unsigned int seed = 123) : m_randGen(seed) { }

  void seed(unsigned int seedValue)
  {
    m_randGen.seed(seedValue);
  }

  unsigned int randInt(unsigned int min, unsigned int max)
  {
    return uniform(m_randGen, uniform_param_t(min, max));
  }

  /**
   * Get a random permutation of indices of the size of the input vector
   */
  void getRandomizedIndexVector(std::vector<unsigned int>& randomOrder)
  {
    unsigned int size = randomOrder.size();
    for (unsigned int i = 0; i < size; ++i)
      randomOrder[i] = i;
    for (unsigned int i = 0; i < size; ++i)
      std::swap(randomOrder[i], randomOrder[i + randInt(0, size - i - 1)]);
  }
};
} // namespace infomap

#else // PYTHON

#include "MersenneTwister.h"

namespace infomap {

class Random {
  MTRand m_randGen;

public:
  Random(unsigned int seed = 123) : m_randGen(seed) { }

  void seed(unsigned int seedValue)
  {
    m_randGen.seed(seedValue);
  }

  unsigned int randInt(unsigned int min, unsigned int max)
  {
    return m_randGen.randInt(max - min) + min;
  }

  void getRandomizedIndexVector(std::vector<unsigned int>& randomOrder)
  {
    unsigned int size = randomOrder.size();
    for (unsigned int i = 0; i < size; ++i)
      randomOrder[i] = i;
    for (unsigned int i = 0; i < size; ++i)
      std::swap(randomOrder[i], randomOrder[i + randInt(0, size - i - 1)]);
  }
};

} // namespace infomap

#endif

#endif /* RANDOM_H_ */
