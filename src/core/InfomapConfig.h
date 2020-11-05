/*
 * InfomapConfig.h
 *
 *  Created on: 2 mar 2015
 *      Author: Daniel
 */

#ifndef MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_
#define MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_

#include "../io/Config.h"
#include "../utils/Random.h"
#include "../utils/Log.h"

#include <string>

namespace infomap {

template <typename Infomap>
class InfomapConfig : public Config {
public:
  InfomapConfig() = default;

  InfomapConfig(const std::string flags) : InfomapConfig(Config(flags)) {}

  InfomapConfig(const Config& conf) : Config(conf), m_rand(conf.seedToRandomNumberGenerator)
  {
    Log::precision(conf.verboseNumberPrecision);
  }

  virtual ~InfomapConfig() = default;

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
    *this = conf;
    m_rand.seed(conf.seedToRandomNumberGenerator);
    Log::precision(conf.verboseNumberPrecision);
    return get();
  }

  Infomap& setNonMainConfig(const Config& conf)
  {
    cloneAsNonMain(conf);
    return get();
  }

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

  Infomap& reseed(unsigned int seed)
  {
    m_rand.seed(seed);
    return get();
  }
};

} // namespace infomap

#endif /* MODULES_CLUSTERING_CLUSTERING_INFOMAPCONFIG_H_ */
