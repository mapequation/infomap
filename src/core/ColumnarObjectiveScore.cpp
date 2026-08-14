/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

// Scoring a stacked hierarchy: the total codelength of the partition the search
// currently holds, under whichever base objective is selected.
//
// This is the *cold* half of the objective. It runs once per candidate structural
// operator (split, merge, layer refine) — O(units) work each time — as opposed to
// the per-candidate move arithmetic in ColumnarObjective.h, which runs millions of
// times per trial. Naming the two objectives as separate functions here therefore
// costs nothing measurable, and it keeps the shared half shared: both read the
// same levels, the same child->parent assignments and the same
// teleport-augmented boundary rates, which is exactly what StackTerms holds.

#include "ColumnarMapEquation.h"
#include "ColumnarObjective.h"
#include "../utils/infomath.h"

#include <vector>

namespace infomap {

using namespace columnar;

namespace {

  /**
   * Everything a stack scoring reads, resolved once per call.
   *
   * The objectives differ in how they combine these terms, not in which terms
   * they need — so this is the shared preamble that both scorers below consume,
   * rather than a branch inside one long function. `enter`/`exit` return the
   * boundary rates with recorded teleportation already folded in, so neither
   * scorer has to know whether the flow model records teleportation.
   */
  struct StackTerms {
    const ColumnarLevel& leaves; // level 0
    const std::vector<ColumnarLevel>& levels; // indices 1..topLevel
    const std::vector<std::vector<int>>& assign; // assign[k]: level k -> level k+1
    int topLevel;
    // Flow leaving the (sub-)network: 0 for the whole closed network, the module's
    // exit flow for an in-context refine. L* charges the top modules' exit
    // codebooks against it; the base index term does not read it.
    double exitNetworkFlow;
    bool tele;
    // Per-module-level teleport enter/exit additions; empty unless `tele`.
    std::vector<std::vector<double>> teleEnter, teleExit;

    const ColumnarLevel& level(int k) const { return k == 0 ? leaves : levels[k]; }
    double enter(int k, int m) const { return level(k).enter[m] + (tele ? teleEnter[k][m] : 0.0); }
    double exit(int k, int m) const { return level(k).exit[m] + (tele ? teleExit[k][m] : 0.0); }
  };

  /**
   * The base map equation over the stack: every internal node codes its children.
   *
   * Level-1 modules code their leaf children (module-of-leaf-nodes term), higher
   * levels code their module children (module-of-modules term), and the root codes
   * the top modules (the index term).
   */
  double scoreStackBase(const StackTerms& t)
  {
    using infomath::plogp;
    double total = 0.0;

    // Level-1 modules code their leaf children (module-of-leaf-nodes term).
    {
      const ColumnarLevel& L1 = t.level(1);
      const std::vector<int>& leafToL1 = t.assign[0];
      std::vector<double> T(L1.n);
      for (int m = 0; m < L1.n; ++m)
        T[m] = L1.flow[m] + t.exit(1, m);
      std::vector<double> acc(L1.n, 0.0);
      for (int i = 0; i < t.leaves.n; ++i) {
        const int m = leafToL1[i];
        if (T[m] >= 1e-16)
          acc[m] -= plogp(t.leaves.flow[i] / T[m]);
      }
      for (int m = 0; m < L1.n; ++m) {
        if (T[m] < 1e-16)
          continue;
        acc[m] -= plogp(t.exit(1, m) / T[m]);
        total += acc[m] * T[m];
      }
    }

    // Higher module levels code their module children (module-of-modules term).
    for (int lvl = 2; lvl <= t.topLevel; ++lvl) {
      const ColumnarLevel& Lk = t.level(lvl);
      const ColumnarLevel& Lkm1 = t.level(lvl - 1);
      const std::vector<int>& childToParent = t.assign[lvl - 1];
      std::vector<double> sumEnter(Lk.n, 0.0), sumPlogpEnter(Lk.n, 0.0);
      for (int c = 0; c < Lkm1.n; ++c) {
        const int p = childToParent[c];
        const double ec = t.enter(lvl - 1, c);
        sumEnter[p] += ec;
        sumPlogpEnter[p] += plogp(ec);
      }
      for (int m = 0; m < Lk.n; ++m) {
        if (Lk.flow[m] < 1e-16)
          continue;
        const double ex = t.exit(lvl, m);
        const double totalUse = ex + sumEnter[m];
        total += plogp(totalUse) - sumPlogpEnter[m] - plogp(ex);
      }
    }

    // Root codes the topmost modules (exit of the whole network is 0).
    {
      const ColumnarLevel& Ltop = t.level(t.topLevel);
      double sumEnter = 0.0, sumPlogpEnter = 0.0;
      for (int m = 0; m < Ltop.n; ++m) {
        const double e = t.enter(t.topLevel, m);
        sumEnter += e;
        sumPlogpEnter += plogp(e);
      }
      total += plogp(sumEnter) - sumPlogpEnter;
    }

    return total;
  }

  /**
   * The non-redundant map equation L* over the stack.
   *
   * Each internal node contributes its calcCodelength, mirroring
   * InfomapBase::calcCodelengthOnTree:
   *  (1) a leaf module (level 1) charges its enter + within codebooks over its
   *      leaf-flow distribution (F = sum plogp(leaf flow));
   *  (2) every super parent (a level 2..top module) charges its own enter
   *      codebook over its children's enter rates, plus each child's leave-one-out
   *      exit codebook (normalized over the parent's children + the parent's exit);
   *  (3) the root (parent of the top modules) has no enter codebook — its enter is
   *      the network's exitNetworkFlow (0 for the closed whole network) — and
   *      charges the top modules' exit codebooks (leave-one-out over the top
   *      siblings, e = exitNetworkFlow). This replaces the base index term: L*
   *      codes the module transition once, in the exit codebook of the module left.
   */
  double scoreStackNonRedundant(const StackTerms& t)
  {
    using infomath::plogp;
    double total = 0.0;

    // (1) Level-1 leaf modules: enter + within over their leaves.
    {
      const ColumnarLevel& L1 = t.level(1);
      const std::vector<int>& leafToL1 = t.assign[0];
      std::vector<double> F(L1.n, 0.0);
      for (int i = 0; i < t.leaves.n; ++i)
        F[leafToL1[i]] += plogp(t.leaves.flow[i]);
      for (int m = 0; m < L1.n; ++m)
        total += nrEnterWithin(L1.flow[m], t.enter(1, m), t.exit(1, m), F[m]);
    }

    // (2) Super parents (level 2..top): parent enter codebook + children exit codebooks.
    for (int k = 2; k <= t.topLevel; ++k) {
      const ColumnarLevel& Lk = t.level(k);
      const ColumnarLevel& Lkm1 = t.level(k - 1);
      const std::vector<int>& childToParent = t.assign[k - 1];
      std::vector<double> sumEnter(Lk.n, 0.0), sumEnterLog(Lk.n, 0.0);
      for (int c = 0; c < Lkm1.n; ++c) {
        const double ec = t.enter(k - 1, c);
        sumEnter[childToParent[c]] += ec;
        sumEnterLog[childToParent[c]] += plogp(ec);
      }
      for (int p = 0; p < Lk.n; ++p) {
        if (sumEnter[p] > 1e-16) // parent's enter codebook: pick a child by enter rate
          total += t.enter(k, p) * (plogp(sumEnter[p]) - sumEnterLog[p]) / sumEnter[p];
      }
      for (int c = 0; c < Lkm1.n; ++c) { // each child's leave-one-out exit codebook
        const int p = childToParent[c];
        const double e = t.exit(k, p);
        total += nrExitTerm(t.enter(k - 1, c), t.exit(k - 1, c), sumEnter[p] + e, sumEnterLog[p], e);
      }
    }

    // (3) Root: the top modules' exit codebooks (leave-one-out over top siblings).
    {
      const ColumnarLevel& Ltop = t.level(t.topLevel);
      double sumEnter = 0.0, sumEnterLog = 0.0;
      for (int m = 0; m < Ltop.n; ++m) {
        const double e = t.enter(t.topLevel, m);
        sumEnter += e;
        sumEnterLog += plogp(e);
      }
      const double e = t.exitNetworkFlow; // 0 for the whole closed network
      for (int m = 0; m < Ltop.n; ++m)
        total += nrExitTerm(t.enter(t.topLevel, m), t.exit(t.topLevel, m), sumEnter + e, sumEnterLog, e);
    }

    return total;
  }

} // namespace

double ColumnarTwoLevel::hierarchicalCodelengthFromStack() const
{
  const int topLevel = static_cast<int>(m_hierLevels.size()) - 1; // >= 1

  StackTerms terms { leaf0(), m_hierLevels, m_hierAssign, topLevel, m_exitNetworkFlow, m_recordedTeleport, {}, {} };

  // Recorded-teleportation enter/exit additions per module level (1..topLevel).
  // A module gains teleport exit = teleFlow_m * (1 - teleWeight_m) and teleport
  // enter = (totalTele - teleFlow_m) * teleWeight_m, from its members' aggregated
  // teleport flow/weight -- exactly InfomapBase::aggregateFlowValuesFromLeafToRoot's
  // recorded-teleportation pass. Empty (and skipped) for the base flow model.
  if (m_recordedTeleport) {
    terms.teleEnter.resize(topLevel + 1);
    terms.teleExit.resize(topLevel + 1);
    std::vector<int> leafToK = m_hierAssign[0]; // leaf -> level-1 module
    for (int k = 1; k <= topLevel; ++k) {
      const int n = hierLevel(k).n;
      std::vector<double> tf(n, 0.0), tw(n, 0.0);
      for (int i = 0; i < m_nLeaves; ++i) {
        tf[leafToK[i]] += m_leafTeleFlow[i];
        tw[leafToK[i]] += m_leafTeleWeight[i];
      }
      terms.teleEnter[k].assign(n, 0.0);
      terms.teleExit[k].assign(n, 0.0);
      for (int m = 0; m < n; ++m) {
        terms.teleEnter[k][m] = (m_totalTeleFlow - tf[m]) * tw[m];
        terms.teleExit[k][m] = tf[m] * (1.0 - tw[m]);
      }
      if (k < topLevel) {
        const std::vector<int>& a = m_hierAssign[k]; // level-k -> level-(k+1)
        for (int i = 0; i < m_nLeaves; ++i)
          leafToK[i] = a[leafToK[i]];
      }
    }
  }

  const double base = m_nonRedundant ? scoreStackNonRedundant(terms) : scoreStackBase(terms);
  return base + objectiveCorrection();
}

double ColumnarTwoLevel::objectiveCorrection() const
{
  if (m_corrections.empty() || m_hierLevels.empty())
    return 0.0;
  double sum = 0.0;
  for (const auto& correction : m_corrections)
    sum += correction->hierarchicalCorrection(*this);
  return sum;
}

} // namespace infomap
