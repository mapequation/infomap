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

  // Recorded-teleportation (tele-path) analogue of OldSideTerms/hoistOldSide:
  // the six old-module (A-side) plogp terms plus the A-side shift of enterFlow
  // are constant across every candidate the move loop probes for the same unit,
  // so the caller hoists them once per unit visit. moduleTeleEnter/moduleTeleExit
  // are inlined here as free lambdas (these helpers live in the class); the
  // per-candidate expression structure below matches deltaCodelengthMovingNodeTele
  // exactly, so results stay bit-identical to the unhoisted form.
  struct TeleOldSideTerms {
    double plogpEA0, plogpXA0, plogpFA0; // before the move
    double plogpEA1, plogpXA1, plogpFA1; // after
    double deltaEnterA; // eA1 - eA0, the A-side contribution to enterFlow1
  };

  inline TeleOldSideTerms hoistOldSideTele(double curEnter, double curExit, double curFlow, double tfu, double twu, double oldEnter, double oldExit, double oldFlow, double oldTeleFlow, double oldTeleWeight, double totalTeleFlow, double deltaOld)
  {
    using infomath::plogp;
    const auto teleEnter = [&](double tf, double tw) { return (totalTeleFlow - tf) * tw; };
    const auto teleExit = [&](double tf, double tw) { return tf * (1.0 - tw); };
    const double eA0 = oldEnter + teleEnter(oldTeleFlow, oldTeleWeight);
    const double xA0 = oldExit + teleExit(oldTeleFlow, oldTeleWeight);
    const double fA0 = xA0 + oldFlow;
    const double eA1 = (oldEnter - curEnter + deltaOld) + teleEnter(oldTeleFlow - tfu, oldTeleWeight - twu);
    const double xA1 = (oldExit - curExit + deltaOld) + teleExit(oldTeleFlow - tfu, oldTeleWeight - twu);
    const double fA1 = xA1 + (oldFlow - curFlow);
    return { plogp(eA0), plogp(xA0), plogp(fA0), plogp(eA1), plogp(xA1), plogp(fA1), eA1 - eA0 };
  }

  inline double deltaCodelengthMovingNodeTeleHoisted(double enterFlow, double enterFlow_log_enterFlow, double curEnter, double curExit, double curFlow, double tfu, double twu, const TeleOldSideTerms& o, double newEnter, double newExit, double newFlow, double newTeleFlow, double newTeleWeight, double totalTeleFlow, double deltaNew)
  {
    using infomath::plogp;
    const auto teleEnter = [&](double tf, double tw) { return (totalTeleFlow - tf) * tw; };
    const auto teleExit = [&](double tf, double tw) { return tf * (1.0 - tw); };
    const double eB0 = newEnter + teleEnter(newTeleFlow, newTeleWeight);
    const double xB0 = newExit + teleExit(newTeleFlow, newTeleWeight);
    const double fB0 = xB0 + newFlow;
    const double eB1 = (newEnter + curEnter - deltaNew) + teleEnter(newTeleFlow + tfu, newTeleWeight + twu);
    const double xB1 = (newExit + curExit - deltaNew) + teleExit(newTeleFlow + tfu, newTeleWeight + twu);
    const double fB1 = xB1 + (newFlow + curFlow);

    const double enterFlow1 = enterFlow + o.deltaEnterA + (eB1 - eB0);
    const double d_enter = plogp(enterFlow1) - enterFlow_log_enterFlow;
    const double d_enter_log = (o.plogpEA1 + plogp(eB1)) - (o.plogpEA0 + plogp(eB0));
    const double d_exit_log = (o.plogpXA1 + plogp(xB1)) - (o.plogpXA0 + plogp(xB0));
    const double d_flow_log = (o.plogpFA1 + plogp(fB1)) - (o.plogpFA0 + plogp(fB0));
    return d_enter - d_enter_log - d_exit_log + d_flow_log;
  }

  // --- Non-redundant map equation (L*) per-module codebook terms ---------------
  // Direct ports of MapEquation::nrEnterWithin / nrExitTerm. F is the module's
  // sum_{leaf in module} plogp(leafFlow). Enter/exit are the module's boundary
  // rates (separate for directed flow). See "Non-redundant map equation - expanded
  // form.ipynb" for the derivation.
  inline double nrEnterWithin(double moduleFlow, double qEnter, double qExit, double F)
  {
    using infomath::plogp;
    if (moduleFlow < 1e-16)
      return 0.0;
    const double tEnter = qEnter * (plogp(moduleFlow) - F) / moduleFlow;
    double tWithin = 0.0;
    const double T = moduleFlow + qExit;
    if (T > 1e-16) {
      const double usage = moduleFlow + qExit - qEnter; // = moduleFlow when undirected
      tWithin = (usage / T) * (plogp(T) - F - plogp(qExit));
    }
    return tEnter + tWithin;
  }

  // The rate at which nrEnterWithin consumes F -- i.e. -d(nrEnterWithin)/dF.
  //
  // nrEnterWithin is affine in F, so this is exact:
  //   nrEnterWithin(f, e, x, F2) - nrEnterWithin(f, e, x, F1)
  //     == -nrLeafCodebookRate(f, e, x) * (F2 - F1).
  // Closed form: 1 + qEnter*qExit / (moduleFlow * (moduleFlow + qExit)) >= 1, with
  // equality iff qEnter == 0 or qExit == 0 (both hold for a one-module partition).
  //
  // Why this exists: a correction that SUBSTITUTES one F for another inside the
  // leaf-module codebook -- MemCorrection swaps the state-node F for the
  // physical-node F -- has to charge the substitution at whatever rate the active
  // objective consumes F. The base map equation's leaf-module term consumes it at
  // exactly 1 (see scoreStackBase, where the T-normalized module term collapses to
  // plogp(T) - plogp(qExit) - F); L* does not, because it splits that codebook into
  // an enter codebook normalized by moduleFlow and a within codebook normalized by
  // T = moduleFlow + qExit. Keep this next to nrEnterWithin: the two must move
  // together, and the 1e-16 guards below mirror it exactly (a module the scorer
  // skips consumes no F at all, so its rate is 0, not 1).
  inline double nrLeafCodebookRate(double moduleFlow, double qEnter, double qExit)
  {
    if (moduleFlow < 1e-16)
      return 0.0; // nrEnterWithin returns 0.0 here: F never enters the total
    double rate = qEnter / moduleFlow; // from tEnter
    const double T = moduleFlow + qExit;
    if (T > 1e-16)
      rate += (moduleFlow + qExit - qEnter) / T; // from tWithin
    return rate;
  }

  // One module's leave-one-out exit codebook contribution, given the sibling
  // enter-rate total-plus-exit-network sumEnterPlusE = (sum_b qEnter_b) + e and
  // sumEnterLogEnter = sum_b plogp(qEnter_b); e is the exit-to-parent codeword.
  inline double nrExitTerm(double qEnter, double qExit, double sumEnterPlusE, double sumEnterLogEnter, double e)
  {
    using infomath::plogp;
    const double Z = sumEnterPlusE - qEnter;
    if (Z < 1e-16)
      return 0.0;
    return qExit * (plogp(Z) - (sumEnterLogEnter - plogp(qEnter)) - plogp(e)) / Z;
  }

} // namespace columnar
} // namespace infomap

#endif // COLUMNAR_OBJECTIVE_H_
