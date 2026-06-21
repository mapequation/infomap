/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMAP_CONFIG_H_
#define INFOMAP_CONFIG_H_

#include "../io/Config.h"
#include "../utils/Random.h"
#include "../utils/Log.h"
#include <string>
#include <utility>

namespace infomap {

template <typename Infomap>
class InfomapConfig : public Config {
public:
  InfomapConfig() = default;

  InfomapConfig(const std::string& flags, bool isCli = false) : InfomapConfig(Config(flags, isCli)) {}

  InfomapConfig(const Config& conf) : Config(conf), m_rand(conf.seedToRandomNumberGenerator)
  {
    Log::precision(conf.verboseNumberPrecision);
  }

  virtual ~InfomapConfig() = default;

  InfomapConfig(const InfomapConfig&) = default;
  InfomapConfig& operator=(const InfomapConfig&) = default;
  InfomapConfig(InfomapConfig&&) noexcept = default;
  InfomapConfig& operator=(InfomapConfig&&) noexcept = default;

private:
  Infomap& get()
  {
    return static_cast<Infomap&>(*this);
  }

protected:
  Random m_rand;

public:
  Config& getConfig()
  {
    return *this;
  }

  const Config& getConfig() const
  {
    return *this;
  }

  Infomap& setConfig(const Config& conf)
  {
    // Assign only the Config base so an injected RNG engine on m_rand survives a
    // config update (a plain `*this = conf` would convert conf to a temporary
    // InfomapConfig and overwrite m_rand with a fresh default engine). The
    // default-engine case is unchanged: m_rand stays default and is reseeded. (#411)
    static_cast<Config&>(*this) = conf;
    m_rand.seed(conf.seedToRandomNumberGenerator);
    Log::precision(conf.verboseNumberPrecision);
    return get();
  }

  Infomap& setNonMainConfig(const Config& conf)
  {
    cloneAsNonMain(conf);
    return get();
  }

#ifndef SWIG
  // Sub/non-main Infomaps are created from the Config base, so they would start
  // with a fresh default RNG. This overload propagates the parent's m_rand
  // (cloning any injected engine) so the whole run tree shares the engine, then
  // reseeds deterministically — matching the default behaviour when no engine is
  // injected. Native-only; not exposed through the language bindings. (#411)
  Infomap& setNonMainConfig(const InfomapConfig& conf)
  {
    cloneAsNonMain(conf);
    m_rand = conf.m_rand;
    m_rand.seed(conf.seedToRandomNumberGenerator);
    return get();
  }

  /**
   * Install a seedable, standard-engine-compatible RNG for native embedders
   * (e.g. igraph), keeping Infomap's default behaviour unchanged when unused.
   *
   * Example:
   *   im.setRandomEngine(MyEngine{});
   *   im.reseed(123);
   */
  template <typename Engine>
  Infomap& setRandomEngine(Engine engine)
  {
    m_rand.setEngine(std::move(engine));
    m_rand.seed(seedToRandomNumberGenerator);
    return get();
  }
#endif

  Infomap& setNumTrials(unsigned int N)
  {
    numTrials = N;
    return get();
  }

  Infomap& setVerbosity(unsigned int level)
  {
    verbosity = level;
    return get();
  }

  Infomap& setTwoLevel(bool value)
  {
    twoLevel = value;
    return get();
  }

  Infomap& setTuneIterationLimit(unsigned int value)
  {
    tuneIterationLimit = value;
    return get();
  }

  Infomap& setFastHierarchicalSolution(unsigned int level)
  {
    fastHierarchicalSolution = level;
    return get();
  }

  Infomap& setOnlySuperModules(bool value)
  {
    onlySuperModules = value;
    return get();
  }

  Infomap& setNoCoarseTune(bool value)
  {
    noCoarseTune = value;
    return get();
  }

  Infomap& setNoInfomap(bool value = true)
  {
    noInfomap = value;
    return get();
  }

  Infomap& setMarkovTime(double codeRate)
  {
    markovTime = codeRate;
    return get();
  }

  Infomap& setDirected(bool value)
  {
    directed = value;
    if (directed && isUndirectedFlow()) {
      flowModel = FlowModel::directed;
    } else if (!directed && !isUndirectedFlow()) {
      flowModel = FlowModel::undirected;
    }
    return get();
  }

  Infomap& reseed(unsigned int seed)
  {
    m_rand.seed(seed);
    return get();
  }
};

} // namespace infomap

#endif // INFOMAP_CONFIG_H_
