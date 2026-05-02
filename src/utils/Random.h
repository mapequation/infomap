/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef RANDOM_H_
#define RANDOM_H_

#include <memory>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace infomap {
using RandGen = std::mt19937;
using uniform_uint_dist = std::uniform_int_distribution<unsigned int>;
using uniform_param_t = uniform_uint_dist::param_type;

class Random {
  struct EngineConcept {
    virtual ~EngineConcept() = default;
    virtual std::unique_ptr<EngineConcept> clone() const = 0;
    virtual void seed(unsigned int seedValue) = 0;
    virtual unsigned int randInt(unsigned int min, unsigned int max) = 0;
  };

  template <typename Engine>
  struct EngineModel : EngineConcept {
    Engine engine;
    uniform_uint_dist uniform;

    explicit EngineModel(Engine engineValue)
      : engine(std::move(engineValue))
    {
    }

    std::unique_ptr<EngineConcept> clone() const override
    {
      return std::unique_ptr<EngineConcept>(new EngineModel(engine));
    }

    void seed(unsigned int seedValue) override
    {
      engine.seed(seedValue);
    }

    unsigned int randInt(unsigned int min, unsigned int max) override
    {
      return uniform(engine, uniform_param_t(min, max));
    }
  };

  RandGen m_defaultEngine;
  uniform_uint_dist m_uniform;
  std::unique_ptr<EngineConcept> m_customEngine;

public:
  Random(unsigned int seed = 123)
    : m_defaultEngine(seed)
  {
  }

  Random(const Random& other)
    : m_defaultEngine(other.m_defaultEngine)
    , m_customEngine(other.m_customEngine ? other.m_customEngine->clone() : nullptr)
  {
  }

  Random& operator=(const Random& other)
  {
    if (this != &other) {
      m_defaultEngine = other.m_defaultEngine;
      m_customEngine = other.m_customEngine ? other.m_customEngine->clone() : nullptr;
    }
    return *this;
  }

  Random(Random&&) noexcept = default;
  Random& operator=(Random&&) noexcept = default;

#ifndef SWIG
  template <typename Engine>
  void setEngine(Engine engine)
  {
    using EngineType = typename std::decay<Engine>::type;
    static_assert(std::is_copy_constructible<EngineType>::value,
                  "Random::setEngine() requires a copy-constructible engine because Random is copyable.");
    m_customEngine.reset(new EngineModel<EngineType>(std::move(engine)));
  }
#endif

  void seed(unsigned int seedValue)
  {
    if (m_customEngine) {
      m_customEngine->seed(seedValue);
      return;
    }
    m_defaultEngine.seed(seedValue);
  }

  unsigned int randInt(unsigned int min, unsigned int max)
  {
    if (m_customEngine)
      return m_customEngine->randInt(min, max);
    return m_uniform(m_defaultEngine, uniform_param_t(min, max));
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

#endif // RANDOM_H_
