/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

// The composable objective corrections (ColumnarCorrection implementations):
// entropy bias, preferred number of modules, metadata, memory/physical codebook,
// and lossy. Each is an additive term on the base map equation plus, where the
// objective shapes the leaf partition, the move-loop and merge hooks that make
// the search itself correction-aware. No search machinery lives here.

#include "ColumnarMapEquation.h"
#include "ColumnarTuning.h"
#include "../utils/infomath.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace infomap {

using namespace columnar;

double BiasedEntropyCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core, StackBreakdown* breakdown) const
{
  // Sum of childDegree over all internal nodes incl. root == count of non-root
  // nodes == sum of every level's size (leaves + modules at all levels).
  long long nonRootNodes = 0;
  const unsigned int levels = core.hierNumLevels();
  for (unsigned int k = 0; k < levels; ++k)
    nonRootNodes += core.hierLevelSize(static_cast<int>(k));

  // Per-node split: the identity above IS a per-node sum, so charge each internal
  // node its own childDegree — the same attribution BiasedMapEquation::calcCodelength
  // makes on the object-oriented tree. Reporting only; the total is unchanged.
  if (breakdown != nullptr) {
    const double perChild = m_multiplier / (2.0 * m_totalDegree);
    const int topLevel = static_cast<int>(levels) - 1;
    for (int k = 1; k <= topLevel; ++k) {
      const int childCount = core.hierLevelSize(k - 1);
      for (int c = 0; c < childCount; ++c)
        breakdown->moduleTerm[static_cast<std::size_t>(k)][static_cast<std::size_t>(core.hierUnitParent(k - 1, c))] += perChild;
    }
    breakdown->rootTerm += perChild * core.hierLevelSize(topLevel);
  }
  return m_multiplier * static_cast<double>(nonRootNodes) / (2.0 * m_totalDegree);
}

// ===================================================
// PreferredModulesCorrection (--preferred-number-of-modules: |K - K_pref| bias)
// ===================================================

double PreferredModulesCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core, StackBreakdown* breakdown) const
{
  // Leaf-module level, matching where the move-loop bias acts (and the other
  // corrections apply). <2 levels means no modules; the base handles that and
  // the one-level fallback would collapse it anyway.
  if (core.hierNumLevels() < 2)
    return 0.0;
  const double penalty = cost(core.hierLevelSize(1));
  // |K - K_pref| is one global scalar about the partition as a whole, not a sum
  // over modules — there is nothing to split, and splitting it evenly would invent
  // a per-module structure the penalty does not have. Convention: charge it to the
  // root, i.e. it shows up as index bits on level 1 of the per-level table.
  if (breakdown != nullptr)
    breakdown->rootTerm += penalty;
  return penalty;
}

double PreferredModulesCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  m_moduleMembers.assign(numModules, 0);
  for (int m : leafModule)
    ++m_moduleMembers[m];
  m_numNonEmpty = 0;
  for (int c : m_moduleMembers)
    if (c > 0)
      ++m_numNonEmpty;
  return cost(m_numNonEmpty);
}

double PreferredModulesCorrection::moveDelta(int /*leaf*/, int oldMod, int newMod) const
{
  if (oldMod == newMod)
    return 0.0;
  // K changes only when the move empties the old module or fills an empty new
  // one (exactly BiasedMapEquation::getDeltaNumModulesIfMoving).
  const int deltaK = (m_moduleMembers[oldMod] == 1 ? -1 : 0) + (m_moduleMembers[newMod] == 0 ? 1 : 0);
  if (deltaK == 0)
    return 0.0;
  return cost(m_numNonEmpty + deltaK) - cost(m_numNonEmpty);
}

void PreferredModulesCorrection::applyMove(int /*leaf*/, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  if (m_moduleMembers[newMod] == 0)
    ++m_numNonEmpty;
  ++m_moduleMembers[newMod];
  --m_moduleMembers[oldMod];
  if (m_moduleMembers[oldMod] == 0)
    --m_numNonEmpty;
}

double PreferredModulesCorrection::mergeDelta(int a, int b) const
{
  // Folding module A into B drops K by one only when both are non-empty.
  const int deltaK = (m_moduleMembers[a] > 0 && m_moduleMembers[b] > 0) ? -1 : 0;
  if (deltaK == 0)
    return 0.0;
  return cost(m_numNonEmpty + deltaK) - cost(m_numNonEmpty);
}

void PreferredModulesCorrection::applyMerge(int a, int b)
{
  if (m_moduleMembers[a] == 0)
    return;
  if (m_moduleMembers[b] == 0) {
    // Non-empty A into empty B: B gains A's members as A empties — K unchanged.
    m_moduleMembers[b] = m_moduleMembers[a];
  } else {
    m_moduleMembers[b] += m_moduleMembers[a];
    --m_numNonEmpty;
  }
  m_moduleMembers[a] = 0;
}

double MetaCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core, StackBreakdown* breakdown) const
{
  using infomath::plogp;
  // Meta term applies at the leaf-module level (level 1): for each bottom
  // module, metaDataRate * F_m * (-sum_c plogp(f_c / F_m)) == metaDataRate *
  // (plogp(F_m) - sum_c plogp(f_c)), matching MetaCollection::calculateEntropy
  // summed over leaf modules.
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  const int numModules = core.hierLevelSize(1);

  std::vector<double> moduleFlow(numModules, 0.0);
  std::unordered_map<long long, double> catFlow; // key = (module<<32)|category
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    moduleFlow[m] += m_leafWeight[i];
    const long long key = (static_cast<long long>(m) << 32) | static_cast<unsigned int>(m_leafCategory[i]);
    catFlow[key] += m_leafWeight[i];
  }

  double total = 0.0;
  for (int m = 0; m < numModules; ++m)
    total += plogp(moduleFlow[m]);
  for (const auto& kv : catFlow)
    total -= plogp(kv.second);
  // Per-node split: the meta term is a sum over leaf modules, so it charges the
  // level-1 modules. Summed separately from `total` (whose two-loop order is part
  // of the reported number and stays untouched), so the two can differ in the last
  // ulp; that is reporting noise, not a second objective.
  if (breakdown != nullptr) {
    for (int m = 0; m < numModules; ++m)
      breakdown->moduleTerm[1][static_cast<std::size_t>(m)] += m_metaDataRate * plogp(moduleFlow[m]);
    for (const auto& kv : catFlow)
      breakdown->moduleTerm[1][static_cast<std::size_t>(kv.first >> 32)] -= m_metaDataRate * plogp(kv.second);
  }
  return m_metaDataRate * total;
}

double MetaCorrection::moduleCategoryFlow(int module, int category) const
{
  const auto& cats = m_moduleCatFlow[module];
  const auto it = cats.find(category);
  return it == cats.end() ? 0.0 : it->second;
}

void MetaCorrection::setUnits(const std::vector<int>& leafToUnit, int numUnits)
{
  // Per-unit sparse (category -> weight) aggregates + total weight, ascending
  // category id for deterministic iteration.
  std::vector<std::unordered_map<int, double>> agg(numUnits);
  m_unitWeight.assign(numUnits, 0.0);
  const int nLeaves = static_cast<int>(leafToUnit.size());
  for (int i = 0; i < nLeaves; ++i) {
    agg[leafToUnit[i]][m_leafCategory[i]] += m_leafWeight[i];
    m_unitWeight[leafToUnit[i]] += m_leafWeight[i];
  }
  m_unitCats.assign(numUnits, {});
  for (int u = 0; u < numUnits; ++u) {
    m_unitCats[u].assign(agg[u].begin(), agg[u].end());
    std::sort(m_unitCats[u].begin(), m_unitCats[u].end());
  }
}

void MetaCorrection::resetUnitsToLeaves()
{
  m_unitCats.clear();
  m_unitWeight.clear();
}

double MetaCorrection::initMoveLoop(const std::vector<int>& unitModule, int numModules)
{
  using infomath::plogp;
  m_moduleFlow.assign(numModules, 0.0);
  m_moduleCatFlow.assign(numModules, {});
  const int nUnits = static_cast<int>(unitModule.size());
  if (m_unitCats.empty()) {
    // Units are leaves.
    for (int i = 0; i < nUnits; ++i) {
      const int m = unitModule[i];
      m_moduleFlow[m] += m_leafWeight[i];
      m_moduleCatFlow[m][m_leafCategory[i]] += m_leafWeight[i];
    }
  } else {
    for (int u = 0; u < nUnits; ++u) {
      const int m = unitModule[u];
      m_moduleFlow[m] += m_unitWeight[u];
      for (const auto& cw : m_unitCats[u])
        m_moduleCatFlow[m][cw.first] += cw.second;
    }
  }
  double total = 0.0;
  for (int m = 0; m < numModules; ++m)
    total += plogp(m_moduleFlow[m]);
  for (const auto& cats : m_moduleCatFlow)
    for (const auto& kv : cats)
      total -= plogp(kv.second);
  return m_metaDataRate * total;
}

double MetaCorrection::moveDelta(int unit, int oldMod, int newMod) const
{
  using infomath::plogp;
  if (oldMod == newMod)
    return 0.0;
  // term(m) = plogp(F_m) - sum_c plogp(f_c). Only F and the unit's categories
  // change in the two modules.
  const double Fo = m_moduleFlow[oldMod], Fn = m_moduleFlow[newMod];
  if (m_unitCats.empty()) {
    const int q = m_leafCategory[unit];
    const double w = m_leafWeight[unit];
    // The old-module delta is identical for every candidate the move loop probes
    // for the same leaf, so it (and plogp(w)) is computed once and cached; a
    // candidate module lacking category q (the common case) needs no category
    // logs — plogp(0) == 0 and plogp(0 + w) == plogp(w), cached alongside.
    if (unit != m_cacheUnit || oldMod != m_cacheOldMod) {
      const double fo = moduleCategoryFlow(oldMod, q);
      m_cacheUnit = unit;
      m_cacheOldMod = oldMod;
      m_cacheDOld = (plogp(Fo - w) - plogp(Fo)) - (plogp(fo - w) - plogp(fo));
      m_cachePlogpW = plogp(w);
    }
    const double fn = moduleCategoryFlow(newMod, q);
    const double dNew = (plogp(Fn + w) - plogp(Fn))
        - (fn == 0.0 ? m_cachePlogpW : (plogp(fn + w) - plogp(fn)));
    return m_metaDataRate * (m_cacheDOld + dNew);
  }
  const double w = m_unitWeight[unit];
  double d = (plogp(Fo - w) - plogp(Fo)) + (plogp(Fn + w) - plogp(Fn));
  for (const auto& cw : m_unitCats[unit]) {
    const double fo = moduleCategoryFlow(oldMod, cw.first), fn = moduleCategoryFlow(newMod, cw.first);
    d -= (plogp(fo - cw.second) - plogp(fo)) + (plogp(fn + cw.second) - plogp(fn));
  }
  return m_metaDataRate * d;
}

void MetaCorrection::applyMove(int unit, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  m_cacheUnit = -1; // module contents change: drop the per-leaf delta cache
  auto moveWeight = [&](int q, double w) {
    auto& oldCats = m_moduleCatFlow[oldMod];
    auto oit = oldCats.find(q);
    if (oit != oldCats.end()) {
      oit->second -= w;
      if (oit->second <= 1e-16)
        oldCats.erase(oit);
    }
    m_moduleCatFlow[newMod][q] += w;
  };
  if (m_unitCats.empty()) {
    const double w = m_leafWeight[unit];
    m_moduleFlow[oldMod] -= w;
    m_moduleFlow[newMod] += w;
    moveWeight(m_leafCategory[unit], w);
    return;
  }
  const double w = m_unitWeight[unit];
  m_moduleFlow[oldMod] -= w;
  m_moduleFlow[newMod] += w;
  for (const auto& cw : m_unitCats[unit])
    moveWeight(cw.first, cw.second);
}

double MetaCorrection::mergeDelta(int a, int b) const
{
  using infomath::plogp;
  // contribution(m) = metaDataRate * (plogp(F_m) - sum_c plogp(f_{m,c})).
  const double Fa = m_moduleFlow[a], Fb = m_moduleFlow[b];
  double d = plogp(Fa + Fb) - plogp(Fa) - plogp(Fb); // change in the F term
  const auto& ca = m_moduleCatFlow[a];
  const auto& cb = m_moduleCatFlow[b];
  for (const auto& kv : ca) {
    const auto it = cb.find(kv.first);
    if (it == cb.end())
      continue; // category only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
    const double bv = it->second;
    d -= plogp(kv.second + bv) - plogp(kv.second) - plogp(bv); // -sum_c plogp(f_c)
  }
  return m_metaDataRate * d;
}

void MetaCorrection::applyMerge(int a, int b)
{
  m_cacheUnit = -1;
  m_moduleFlow[b] += m_moduleFlow[a];
  m_moduleFlow[a] = 0.0;
  auto& ca = m_moduleCatFlow[a];
  auto& cb = m_moduleCatFlow[b];
  for (const auto& kv : ca)
    cb[kv.first] += kv.second;
  ca.clear();
}

std::unique_ptr<ColumnarCorrection> MetaCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<int> category(n);
  std::vector<double> weight(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    category[j] = m_leafCategory[g];
    weight[j] = m_leafWeight[g];
  }
  return std::make_unique<MetaCorrection>(std::move(category), std::move(weight), m_metaDataRate);
}

// ===================================================
// MemCorrection (memory / state networks: physical-node codebook)
// ===================================================

MemCorrection::MemCorrection(std::vector<int> leafPhysical, std::vector<double> leafFlow)
    : m_leafPhysical(std::move(leafPhysical)), m_leafFlow(std::move(leafFlow)), m_cState(0.0)
{
  using infomath::plogp;
  for (double f : m_leafFlow)
    m_cState += plogp(f);
  // Compact the physical ids to [0, P): every downstream structure (module
  // maps, reverse index) keys on the compact id, and the dense per-module
  // lookup (see initMoveLoop) indexes with it directly.
  std::unordered_map<int, int> compact;
  compact.reserve(m_leafPhysical.size());
  for (auto& p : m_leafPhysical) {
    const auto r = compact.emplace(p, static_cast<int>(compact.size()));
    p = r.first->second;
  }
  m_numPhys = static_cast<int>(compact.size());
}

double MemCorrection::physFlow(int module, int physical) const
{
  if (m_dense)
    return m_densePhysFlow[static_cast<std::size_t>(module) * m_numPhys + physical];
  const auto& pf = m_modulePhysFlow[module];
  const auto it = pf.find(physical);
  return it == pf.end() ? 0.0 : it->second;
}

double MemCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core, StackBreakdown* breakdown) const
{
  using infomath::plogp;
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  // Per level-1 module, the rate the active objective charges F_m at. Empty ==
  // uniformly 1 == the base map equation; see the class comment for why the two
  // objectives differ here and the header for why empty rather than a vector of ones.
  const std::vector<double> rates = core.leafCodebookRates();
  const bool weighted = !rates.empty();
  std::vector<double> fState;
  if (weighted)
    fState.assign(rates.size(), 0.0);
  std::unordered_map<long long, double> physFlowMap; // key = (module<<32)|physical
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    const long long key = (static_cast<long long>(m) << 32) | static_cast<unsigned int>(m_leafPhysical[i]);
    physFlowMap[key] += m_leafFlow[i];
    if (weighted)
      fState[static_cast<std::size_t>(m)] += plogp(m_leafFlow[i]);
  }
  if (!weighted) {
    // rate == 1 everywhere, so sum_m (F_m^state - F_m^phys) telescopes and the
    // per-module split is pure noise — kept as the two global sums it has always
    // been, since the summation order is part of the reported number.
    double sum = 0.0;
    for (const auto& kv : physFlowMap)
      sum += plogp(kv.second);
    // For reporting the split is not noise, it is the whole point: the physical
    // codebook substitution happens per leaf module, so re-derive it that way
    // rather than dropping the correction on the root. Same value up to summation
    // order, which the returned total keeps.
    if (breakdown != nullptr) {
      const int numModules = core.hierLevelSize(1);
      std::vector<double> fStateSplit(static_cast<std::size_t>(numModules), 0.0);
      for (int i = 0; i < nLeaves; ++i)
        fStateSplit[static_cast<std::size_t>(core.hierLeafModule(i))] += plogp(m_leafFlow[i]);
      for (int m = 0; m < numModules; ++m)
        breakdown->moduleTerm[1][static_cast<std::size_t>(m)] += fStateSplit[static_cast<std::size_t>(m)];
      for (const auto& kv : physFlowMap)
        breakdown->moduleTerm[1][static_cast<std::size_t>(kv.first >> 32)] -= plogp(kv.second);
    }
    return m_cState - sum;
  }
  std::vector<double> fPhys(rates.size(), 0.0);
  for (const auto& kv : physFlowMap)
    fPhys[static_cast<std::size_t>(kv.first >> 32)] += plogp(kv.second);
  double total = 0.0;
  for (std::size_t m = 0; m < rates.size(); ++m) {
    const double term = rates[m] * (fState[m] - fPhys[m]);
    total += term;
    if (breakdown != nullptr)
      breakdown->moduleTerm[1][m] += term;
  }
  return total;
}

void MemCorrection::setUnits(const std::vector<int>& leafToUnit, int numUnits)
{
  // Per-unit sparse (physical -> flow) aggregates, ascending physical id for
  // deterministic iteration.
  std::vector<std::unordered_map<int, double>> agg(numUnits);
  const int nLeaves = static_cast<int>(leafToUnit.size());
  for (int i = 0; i < nLeaves; ++i)
    agg[leafToUnit[i]][m_leafPhysical[i]] += m_leafFlow[i];
  m_unitPhys.assign(numUnits, {});
  for (int u = 0; u < numUnits; ++u) {
    m_unitPhys[u].assign(agg[u].begin(), agg[u].end());
    std::sort(m_unitPhys[u].begin(), m_unitPhys[u].end());
  }
}

void MemCorrection::resetUnitsToLeaves()
{
  m_unitPhys.clear();
}

double MemCorrection::initMoveLoop(const std::vector<int>& unitModule, int numModules)
{
  using infomath::plogp;
  m_modulePhysFlow.assign(numModules, {});
  m_physModules.clear();
  m_cacheUnit = -1;
  // Dense per-module physical-flow lookup when it fits comfortably in memory
  // (state networks typically have far fewer physical nodes than state nodes):
  // physFlow becomes an O(1) array read in the move loop's hot path instead of
  // a hash find. The sparse maps are kept alongside for iteration (merge scan).
  m_dense = static_cast<double>(numModules) * static_cast<double>(m_numPhys) <= 8e6;
  if (m_dense)
    m_densePhysFlow.assign(static_cast<std::size_t>(numModules) * m_numPhys, 0.0);
  // The physical -> modules reverse index only feeds the co-physical proposals
  // (coMergeMode); maintaining it is pure overhead when that tuning mode is off.
  const bool maintainIndex = coMergeMode() != 0;
  const int nUnits = static_cast<int>(unitModule.size());
  if (m_unitPhys.empty()) {
    // Units are leaves.
    for (int i = 0; i < nUnits; ++i) {
      const int m = unitModule[i], p = m_leafPhysical[i];
      m_modulePhysFlow[m][p] += m_leafFlow[i];
      if (m_dense)
        m_densePhysFlow[static_cast<std::size_t>(m) * m_numPhys + p] += m_leafFlow[i];
      if (maintainIndex)
        m_physModules[p].insert(m);
    }
  } else {
    for (int u = 0; u < nUnits; ++u) {
      const int m = unitModule[u];
      for (const auto& pf : m_unitPhys[u]) {
        m_modulePhysFlow[m][pf.first] += pf.second;
        if (m_dense)
          m_densePhysFlow[static_cast<std::size_t>(m) * m_numPhys + pf.first] += pf.second;
        if (maintainIndex)
          m_physModules[pf.first].insert(m);
      }
    }
  }
  double sum = 0.0;
  for (const auto& pf : m_modulePhysFlow)
    for (const auto& kv : pf)
      sum += plogp(kv.second);
  return m_cState - sum;
}

double MemCorrection::moveDelta(int unit, int oldMod, int newMod) const
{
  using infomath::plogp;
  if (oldMod == newMod)
    return 0.0;
  // term = C_state - sum plogp(physFlow); only the unit's physicals in the two
  // modules change. The old-module part is identical for every candidate of the
  // same unit, so it is computed once and cached (the move loop probes many
  // candidates per unit); a candidate without the unit's physical (the common
  // case) needs no logs at all: plogp(0) == 0 and plogp(0 + f) == plogp(f),
  // which is cached alongside.
  if (m_unitPhys.empty()) {
    const int p = m_leafPhysical[unit];
    const double f = m_leafFlow[unit];
    if (unit != m_cacheUnit || oldMod != m_cacheOldMod) {
      const double oc = physFlow(oldMod, p);
      m_cacheUnit = unit;
      m_cacheOldMod = oldMod;
      m_cacheOldTerm = plogp(oc) - plogp(oc - f);
      m_cachePlogpF = plogp(f);
    }
    const double nc = physFlow(newMod, p);
    return m_cacheOldTerm + (nc == 0.0 ? -m_cachePlogpF : (plogp(nc) - plogp(nc + f)));
  }
  double d = 0.0;
  for (const auto& pf : m_unitPhys[unit]) {
    const double oc = physFlow(oldMod, pf.first), nc = physFlow(newMod, pf.first);
    const double dOld = (oc == 0.0) ? 0.0 : (plogp(oc) - plogp(oc - pf.second));
    const double dNew = (nc == 0.0) ? -plogp(pf.second) : (plogp(nc) - plogp(nc + pf.second));
    d += dOld + dNew;
  }
  return d;
}

void MemCorrection::applyMove(int unit, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  m_cacheUnit = -1; // module contents change: drop the per-unit delta cache
  const bool maintainIndex = !m_physModules.empty() || coMergeMode() != 0;
  auto moveFlow = [&](int p, double f) {
    auto& oldPhys = m_modulePhysFlow[oldMod];
    auto it = oldPhys.find(p);
    if (it != oldPhys.end()) {
      it->second -= f;
      if (it->second <= 1e-16) {
        oldPhys.erase(it);
        if (maintainIndex)
          m_physModules[p].erase(oldMod); // oldMod no longer holds physical p
      }
    }
    m_modulePhysFlow[newMod][p] += f;
    if (m_dense) {
      auto& ov = m_densePhysFlow[static_cast<std::size_t>(oldMod) * m_numPhys + p];
      ov -= f;
      if (ov <= 1e-16)
        ov = 0.0; // keep the zero fast path exact
      m_densePhysFlow[static_cast<std::size_t>(newMod) * m_numPhys + p] += f;
    }
    if (maintainIndex)
      m_physModules[p].insert(newMod);
  };
  if (m_unitPhys.empty()) {
    moveFlow(m_leafPhysical[unit], m_leafFlow[unit]);
    return;
  }
  for (const auto& pf : m_unitPhys[unit])
    moveFlow(pf.first, pf.second);
}

void MemCorrection::proposeMoveTargets(int unit, std::vector<int>& out) const
{
  // Modules already holding a state node of this unit's physical node(s) — the
  // co-physical merges that shrink the physical codebook but are (usually) not
  // reachable through the edge neighborhood.
  if (m_unitPhys.empty()) {
    const auto it = m_physModules.find(m_leafPhysical[unit]);
    if (it == m_physModules.end())
      return;
    for (int m : it->second)
      out.push_back(m);
    return;
  }
  const auto base = static_cast<std::ptrdiff_t>(out.size());
  for (const auto& pf : m_unitPhys[unit]) {
    const auto it = m_physModules.find(pf.first);
    if (it == m_physModules.end())
      continue;
    for (int m : it->second)
      out.push_back(m);
  }
  std::sort(out.begin() + base, out.end());
  out.erase(std::unique(out.begin() + base, out.end()), out.end());
}

double MemCorrection::mergeDelta(int a, int b) const
{
  using infomath::plogp;
  // contribution(m) = -sum_p plogp(physFlow_m[p]); merging folds A's physical
  // flows into B. Only physicals present in BOTH modules change the sum (a
  // physical in only one keeps its single plogp term). Iterate the (smaller)
  // A side; b-only physicals are handled when B is the A of another pair.
  const auto& pa = m_modulePhysFlow[a];
  double dSum = 0.0; // change in sum_p plogp(physFlow)
  if (m_dense) {
    const std::size_t bBase = static_cast<std::size_t>(b) * m_numPhys;
    for (const auto& kv : pa) {
      const double bv = m_densePhysFlow[bBase + kv.first];
      if (bv == 0.0)
        continue; // physical only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
      dSum += plogp(kv.second + bv) - plogp(kv.second) - plogp(bv);
    }
  } else {
    const auto& pb = m_modulePhysFlow[b];
    for (const auto& kv : pa) {
      const auto it = pb.find(kv.first);
      if (it == pb.end())
        continue; // physical only in A: plogp(a+0) - plogp(a) - plogp(0) == 0
      dSum += plogp(kv.second + it->second) - plogp(kv.second) - plogp(it->second);
    }
  }
  return -dSum; // correction = C_state - sum_p plogp; delta correction = -dSum
}

void MemCorrection::applyMerge(int a, int b)
{
  m_cacheUnit = -1;
  auto& pa = m_modulePhysFlow[a];
  auto& pb = m_modulePhysFlow[b];
  for (const auto& kv : pa) {
    pb[kv.first] += kv.second;
    if (m_dense) {
      m_densePhysFlow[static_cast<std::size_t>(b) * m_numPhys + kv.first] += kv.second;
      m_densePhysFlow[static_cast<std::size_t>(a) * m_numPhys + kv.first] = 0.0;
    }
    if (!m_physModules.empty()) { // maintained only when co-physical mode is on
      auto& mods = m_physModules[kv.first];
      mods.erase(a);
      mods.insert(b);
    }
  }
  pa.clear();
}

void MemCorrection::proposeMergePartners(int module, std::vector<int>& out) const
{
  // Leaf modules sharing a physical node with `module` (co-physical merge
  // candidates the edge-based set can miss on directed clustering).
  if (module >= static_cast<int>(m_modulePhysFlow.size()))
    return;
  for (const auto& kv : m_modulePhysFlow[module]) {
    const auto it = m_physModules.find(kv.first);
    if (it == m_physModules.end())
      continue;
    for (int m : it->second)
      if (m != module)
        out.push_back(m);
  }
}

std::unique_ptr<ColumnarCorrection> MemCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<int> physical(n);
  std::vector<double> flow(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    physical[j] = m_leafPhysical[g];
    flow[j] = m_leafFlow[g];
  }
  return std::make_unique<MemCorrection>(std::move(physical), std::move(flow));
}

// ===================================================
// LossyCorrection (rate-distortion map equation: noise modules)
// ===================================================

LossyCorrection::LossyCorrection(std::vector<double> leafFlow, std::vector<double> leafEntropy, double lambda)
    : m_leafFlow(std::move(leafFlow)), m_leafEntropy(std::move(leafEntropy)), m_lambda(lambda)
{
  using infomath::plogp;
  m_leafFlf.resize(m_leafFlow.size());
  for (std::size_t i = 0; i < m_leafFlow.size(); ++i)
    m_leafFlf[i] = plogp(m_leafFlow[i]);
}

double LossyCorrection::moduleCost(double flow, double flowLogFlow, double entropy) const
{
  using infomath::plogp;
  // Naming-overhead loss beyond the tolerated distortion; 0 when the module is
  // not noise (matches LossyMapEquation::calcCorrection).
  return std::max(0.0, (plogp(flow) - flowLogFlow) - m_lambda * entropy);
}

double LossyCorrection::hierarchicalCorrection(const ColumnarTwoLevel& core, StackBreakdown* breakdown) const
{
  if (core.hierNumLevels() < 2)
    return 0.0;
  const int nLeaves = core.numLeaves();
  const int numModules = core.hierLevelSize(1);
  std::vector<double> F(numModules, 0.0), flf(numModules, 0.0), H(numModules, 0.0);
  for (int i = 0; i < nLeaves; ++i) {
    const int m = core.hierLeafModule(i);
    F[m] += m_leafFlow[i];
    flf[m] += m_leafFlf[i];
    H[m] += m_leafEntropy[i];
  }
  double sumC = 0.0;
  for (int m = 0; m < numModules; ++m) {
    const double c = moduleCost(F[m], flf[m], H[m]);
    sumC += c;
    // The noise-module discount is already per leaf module; charge it there.
    if (breakdown != nullptr)
      breakdown->moduleTerm[1][static_cast<std::size_t>(m)] -= c;
  }
  return -sumC; // objective J = base - sum_m c_m
}

double LossyCorrection::initMoveLoop(const std::vector<int>& leafModule, int numModules)
{
  m_moduleFlow.assign(numModules, 0.0);
  m_moduleFlf.assign(numModules, 0.0);
  m_moduleEntropy.assign(numModules, 0.0);
  const int nLeaves = static_cast<int>(leafModule.size());
  for (int i = 0; i < nLeaves; ++i) {
    const int m = leafModule[i];
    m_moduleFlow[m] += m_leafFlow[i];
    m_moduleFlf[m] += m_leafFlf[i];
    m_moduleEntropy[m] += m_leafEntropy[i];
  }
  double sumC = 0.0;
  for (int m = 0; m < numModules; ++m)
    sumC += moduleCost(m_moduleFlow[m], m_moduleFlf[m], m_moduleEntropy[m]);
  return -sumC;
}

double LossyCorrection::moveDelta(int leaf, int oldMod, int newMod) const
{
  if (oldMod == newMod)
    return 0.0;
  const double f = m_leafFlow[leaf], lf = m_leafFlf[leaf], h = m_leafEntropy[leaf];
  const double dOld = moduleCost(m_moduleFlow[oldMod] - f, m_moduleFlf[oldMod] - lf, m_moduleEntropy[oldMod] - h)
      - moduleCost(m_moduleFlow[oldMod], m_moduleFlf[oldMod], m_moduleEntropy[oldMod]);
  const double dNew = moduleCost(m_moduleFlow[newMod] + f, m_moduleFlf[newMod] + lf, m_moduleEntropy[newMod] + h)
      - moduleCost(m_moduleFlow[newMod], m_moduleFlf[newMod], m_moduleEntropy[newMod]);
  return -(dOld + dNew); // correction = -sum_m c_m
}

void LossyCorrection::applyMove(int leaf, int oldMod, int newMod)
{
  if (oldMod == newMod)
    return;
  const double f = m_leafFlow[leaf], lf = m_leafFlf[leaf], h = m_leafEntropy[leaf];
  m_moduleFlow[oldMod] -= f;
  m_moduleFlf[oldMod] -= lf;
  m_moduleEntropy[oldMod] -= h;
  m_moduleFlow[newMod] += f;
  m_moduleFlf[newMod] += lf;
  m_moduleEntropy[newMod] += h;
}

double LossyCorrection::mergeDelta(int a, int b) const
{
  const double ca = moduleCost(m_moduleFlow[a], m_moduleFlf[a], m_moduleEntropy[a]);
  const double cb = moduleCost(m_moduleFlow[b], m_moduleFlf[b], m_moduleEntropy[b]);
  const double cab = moduleCost(m_moduleFlow[a] + m_moduleFlow[b],
                                m_moduleFlf[a] + m_moduleFlf[b],
                                m_moduleEntropy[a] + m_moduleEntropy[b]);
  return -(cab - ca - cb); // change in -sum_m c_m
}

void LossyCorrection::applyMerge(int a, int b)
{
  m_moduleFlow[b] += m_moduleFlow[a];
  m_moduleFlf[b] += m_moduleFlf[a];
  m_moduleEntropy[b] += m_moduleEntropy[a];
  m_moduleFlow[a] = m_moduleFlf[a] = m_moduleEntropy[a] = 0.0;
}

std::unique_ptr<ColumnarCorrection> LossyCorrection::sliceForLeaves(const std::vector<int>& globalLeafIds) const
{
  const int n = static_cast<int>(globalLeafIds.size());
  std::vector<double> flow(n), entropy(n);
  for (int j = 0; j < n; ++j) {
    const int g = globalLeafIds[j];
    flow[j] = m_leafFlow[g];
    entropy[j] = m_leafEntropy[g];
  }
  return std::make_unique<LossyCorrection>(std::move(flow), std::move(entropy), m_lambda);
}

} // namespace infomap
