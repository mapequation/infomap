/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNAR_OBJECTIVE_H_
#define COLUMNAR_OBJECTIVE_H_

// The columnar base map equation, as arithmetic: the per-candidate move deltas
// (plain and recorded-teleportation, unhoisted and hoisted), the non-redundant
// map equation's per-module codebook terms, and the calibration constants.
//
// A header, not a .cpp, on purpose. These kernels are the innermost arithmetic
// of the move loop — millions of calls per trial, and the hoisted forms exist
// precisely to shave plogp calls off that path — and the native build has no
// LTO, so a cross-translation-unit call here would cost real time. Defined
// inline, they inline into the move loop exactly as they did when they shared
// its file.
//
// `infomap::columnar` rather than plain `infomap`: MapEquation.h already declares
// an `OldSideTerms` at namespace scope for the object-oriented objectives, and
// the two must not collide in a translation unit that sees both. Consumers open
// this namespace with `using namespace columnar;` so call sites read unqualified.

#include "../utils/infomath.h"

#include <cmath>

namespace infomap {
namespace columnar {

  // Codelength calibration constants, mirroring the Config defaults the OO core
  // uses (the columnar path is a measurement scaffold, not user-tunable).
  constexpr double kMinImprovement = 1e-10;
  constexpr double kMinSingleImprovement = 1e-16;
  constexpr unsigned int kCoreLoopLimit = 10;

  // Flat-first trials (#889): the cheap flat probe (full aggregation, no leaf
  // fine-tune) must land within this relative margin of the fine-blocks
  // up-build's post-build codelength for the trial to complete the expensive
  // leaf-level flat pipeline. Measured across the benchmark set, every network
  // where the flat search truly wins probes at est/build <= 1.000 (air30k
  // 0.84, malaria 0.88, jazz 0.996, lazega 0.995) and every true flat-loser
  // probes at >= 1.008 (science2001 1.008, netsci 1.034, powergrid 1.10,
  // web-NotreDame 1.18) — the post-build refinement gains more than the flat
  // completion does, so a generous margin only buys false positives. 0.5%
  // sits in the middle of the observed gap.
  constexpr double kFlatProbeMargin = 0.005;

  // Deep repair's fresh split discovery (splitTopModules, #1081 / F57). A module
  // holding at least this share of the leaves, next to other modules, is not
  // sub-clustered from singletons: the search converged to it, and that
  // sub-problem is the whole-network search again. Measured: the regularized
  // om5-om8 E100000 winners hold one module of 98.6-99.1% (each re-derivation
  // 0.4 s for <= 0.02% in bits); the largest module on every other benchmark
  // winner holds at most 31%. Decided on the repair's seed, so the one-level
  // fallback (a single module, which no search produced) is repaired in full.
  constexpr double kWholeNetworkModuleShare = 0.95;
  // ...and a fresh derivation is repeated only while the last one bought at least
  // this fraction of the codelength per whole network of leaves it re-clustered
  // (the work a derivation pays, cache misses only). The productive rounds sit
  // far above it (malaria 0.04-0.5%, om2 >= 0.019%, om4 4.6%); the om5 regularized
  // re-draws of a barely changed module at 0.0003%. Near the threshold a round
  // buys <= 0.01% either way, so its sensitivity is in seconds, not bits.
  constexpr double kFreshYieldPerNetwork = 1e-4;
  // The same bar when the repair's seed is one module (the one-level fallback), where
  // the derivations stop at pass-1 blocks: each is cheap and the rounds after the
  // first buy 0.005-0.03%, so 1e-4 lets them run 4-7 times (F59).
  constexpr double kFreshYieldOneModule = 5e-4;
  // A two-level partition whose largest module holds at least this share of the
  // flow has collapsed. The regroup ladder judges it against structured rungs at a
  // granularity only the collapse is immune to, which matters where the collapse is
  // an attractor of the greedy search, as under recorded teleportation (#1042, see
  // optimizeTwoLevel).
  // A flow share, not a leaf share: on the sparse om E50000 networks the collapsed
  // module holds 99.9% of the flow while its crumbs keep many low-flow states.
  // F61 measured the collapsed winners at 0.966-0.975 and every other benchmark
  // winner at <= 0.23.
  constexpr double kCollapsedModuleFlowShare = 0.9;
  // ...and the best non-collapsed candidate is leaf-tuned against it only when
  // its block-granularity codelength is within this fraction of the tuned
  // collapsed winner. Measured over the 98 comparisons of the om family at
  // `-2d --regularized -N10`: every one the candidate won started 0.34-4.57%
  // behind, every one it lost 6.19-12.99% behind (om7 / om8 E100000 and om3-om6
  // E50000 lose all ten), so the bound skips exactly the tunes that cannot win.
  constexpr double kRunnerUpTuneGap = 0.05;
  // An aggregation pass that improves the codelength by less than this fraction
  // is the last one run (it is kept). The stalled passes re-sort a few units for
  // ~0.001% each; the productive ones buy 0.27% or more (om6_N256_E100k
  // `-2d --regularized`). Over the 148 benchmark searches without recorded
  // teleportation it changed one codelength, by -0.003% (#1042).
  constexpr double kAggregationStallGain = 1e-4;

  // Exact port of MapEquation::getDeltaCodelengthOnMovingNode for the base
  // objective: change in codelength from moving a unit out of its old module
  // (aggregates old*) into a candidate module (aggregates new*). deltaOld/deltaNew
  // are the summed enter+exit flow between the unit and the old/new module.
  inline double deltaCodelengthMovingNode(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, double oldEnter, double oldExit, double oldFlow, double newEnter, double newExit, double newFlow, double deltaOld, double deltaNew)
  {
    using infomath::plogp;
    const double oldExitPlusFlow = oldExit + oldFlow;
    const double newExitPlusFlow = newExit + newFlow;

    const double delta_enter = plogp(enterFlow + deltaOld - deltaNew) - enterFlow_log_enterFlow;
    const double delta_enter_log_enter = -plogp(oldEnter) - plogp(newEnter)
        + plogp(oldEnter - curEnter + deltaOld) + plogp(newEnter + curEnter - deltaNew);
    const double delta_exit_log_exit = -plogp(oldExit) - plogp(newExit)
        + plogp(oldExit - curExit + deltaOld) + plogp(newExit + curExit - deltaNew);
    const double delta_flow_log_flow = -plogp(oldExitPlusFlow) - plogp(newExitPlusFlow)
        + plogp(oldExitPlusFlow - curExit - curFlow + deltaOld) + plogp(newExitPlusFlow + curExit + curFlow - deltaNew);

    return delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  }

  // Old-module plogp terms of deltaCodelengthMovingNode: constant for every
  // candidate the move loop probes for the same unit, so the caller computes
  // them once per unit (6 of the 13 plogp calls per candidate hoist out; the
  // per-candidate math below keeps the exact expression structure, so results
  // stay bit-identical to the unhoisted form).
  struct OldSideTerms {
    double plogpOldEnter, plogpOldExit, plogpOldEF;
    double plogpOldEnterAfter, plogpOldExitAfter, plogpOldEFAfter;
  };

  inline OldSideTerms hoistOldSide(double curEnter, double curExit, double curFlow, double oldEnter, double oldExit, double oldFlow, double deltaOld)
  {
    using infomath::plogp;
    const double oldExitPlusFlow = oldExit + oldFlow;
    return { plogp(oldEnter), plogp(oldExit), plogp(oldExitPlusFlow), plogp(oldEnter - curEnter + deltaOld), plogp(oldExit - curExit + deltaOld), plogp(oldExitPlusFlow - curExit - curFlow + deltaOld) };
  }

  inline double deltaCodelengthMovingNodeHoisted(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, const OldSideTerms& o, double newEnter, double newExit, double newFlow, double deltaOld, double deltaNew)
  {
    using infomath::plogp;
    const double newExitPlusFlow = newExit + newFlow;

    const double delta_enter = plogp(enterFlow + deltaOld - deltaNew) - enterFlow_log_enterFlow;
    const double delta_enter_log_enter = -o.plogpOldEnter - plogp(newEnter)
        + o.plogpOldEnterAfter + plogp(newEnter + curEnter - deltaNew);
    const double delta_exit_log_exit = -o.plogpOldExit - plogp(newExit)
        + o.plogpOldExitAfter + plogp(newExit + curExit - deltaNew);
    const double delta_flow_log_flow = -o.plogpOldEF - plogp(newExitPlusFlow)
        + o.plogpOldEFAfter + plogp(newExitPlusFlow + curExit + curFlow - deltaNew);

    return delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
  }

  // --- Generative map equation (Lhat*) per-node terms --------------------------
  // What the non-redundant code actually COSTS on the walk: the cross-entropy of
  // the walk under that code's kernel, as opposed to L* above, which sums the same
  // codebooks' ENTROPIES. The two differ because the non-redundant code is proper
  // but unmatched -- each codebook's model weights are not the frequencies the
  // stream puts through it -- so its entropy is not its price.
  //
  // Derived in the project's primer (Section 8.5 and Appendix A) and verified there
  // against the explicit hierarchical kernel to 9e-16; see
  // test/cpp/test_lhat_star_columnar.cpp for the fixtures this implementation is
  // pinned to. Two properties shape the code:
  //
  //  (1) Only module-level statistics survive. Every pairwise crossing rate cancels
  //      at every internal node, because all flow leaving a child either reaches a
  //      sibling or leaves the parent, and both are coded from the same leave-one-out
  //      index book. So a node's term reads only its own enter/exit rates and its
  //      children's.
  //  (2) The leaf-node entropy H(p) enters as one partition-independent constant
  //      (-sum_i plogp(p_i) = -sum_m F_m). It is folded into the leaf-module term
  //      here so each level-1 module's charge is self-contained, which is also what
  //      makes the per-module decomposition sum to the reported total.
  //
  // Both deletions apply at EVERY level: a node's index book excludes the child the
  // walker just left (normalizer D - qEnter_child), and a book opened on arrival from
  // outside cannot emit `exit' (normalizer E).

  // Level-1 (leaf) module: its within codebook plus its leaves' own entropy.
  // F is the module's sum_{leaf in module} plogp(leafFlow).
  inline double lhatLeafModule(double moduleFlow, double qEnter, double qExit, double F)
  {
    using infomath::log2;
    using infomath::plogp;
    const double Z = moduleFlow + qExit; // the within book's mass: the nodes plus `exit'
    double term = -F; // this module's share of H(p)
    if (Z < 1e-16)
      return term;
    term += moduleFlow * log2(Z) - plogp(qExit);
    if (moduleFlow > 1e-16)
      term += qEnter * log2(moduleFlow); // the enter book names the node arrived at
    return term;
  }

  // One child's contribution to its parent's index book: the child's exit flow is coded
  // in a book normalized leave-one-out (D - qEnter_child), and the child's own name is
  // drawn at rate qEnter_child by whoever addresses it. D = qExit_parent +
  // sum_{c in children} qEnter_c is the full index-book mass.
  inline double lhatChildTerm(double qEnterChild, double qExitChild, double D)
  {
    using infomath::log2;
    using infomath::plogp;
    double term = -plogp(qEnterChild);
    const double Z = D - qEnterChild;
    if (Z > 1e-16)
      term += qExitChild * log2(Z);
    return term;
  }

  // An internal node's own two contributions: the `exit' symbol it draws from its index
  // book, and the descend-only book (mass E = sum_c qEnter_c) it opens for a walker
  // arriving from outside, which cannot leave again before landing.
  inline double lhatNodeTerm(double qEnterSelf, double qExitSelf, double E)
  {
    using infomath::log2;
    using infomath::plogp;
    double term = -plogp(qExitSelf);
    if (E > 1e-16)
      term += qEnterSelf * log2(E);
    return term;
  }

} // namespace columnar
} // namespace infomap

#endif // COLUMNAR_OBJECTIVE_H_
