/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef COLUMNARLEVEL_H_
#define COLUMNARLEVEL_H_

#include <vector>

namespace infomap {

/**
 * One aggregation level of the columnar search: unit flow/enter/exit plus the
 * out and in adjacency as CSR (units = leaves at level 0, then modules, ...).
 * teleFlow/teleWeight are the per-unit recorded-teleportation aggregates (0 for
 * the base flow model); they sum under aggregation like flow, so a unit's
 * module-level teleport enter/exit is a pure function of its members.
 *
 * linkEnter / linkExit carry ONLY the flow crossing the unit's boundary along
 * links. They are NOT the rates the map equation charges. Under recorded
 * teleportation a unit is also entered by teleporting into it from outside, and
 * left by teleporting away, so its index-codebook use rate is
 *
 *     q = linkEnter + (T - teleFlow) * teleWeight      [ColumnarTwoLevel::unitIndexRate]
 *     x = linkExit  + teleFlow * (1 - teleWeight)      [moduleTeleExit]
 *
 * and the teleport half is NOT additive over a group's members -- it is a
 * product of two aggregates, which is exactly why it is kept out of these
 * vectors and recomputed at each use instead of being folded in as the
 * object-oriented core does in aggregateFlowValuesFromLeafToRoot.
 *
 * The price of that representation is that any consumer which reads these
 * fields as if they were rates silently optimizes the wrong quantity -- #1038,
 * where six sites in the search did exactly that and were 43.7% (om5) to 72.0%
 * (om8) off. The fields are named linkEnter/linkExit rather than enter/exit so
 * that a bare read is visibly incomplete at the call site. To get a rate, use
 * ColumnarTwoLevel::unitIndexRate / setIndexRateAsFlow, or the scorer's
 * StackTerms::enter / ::exit, all of which add the teleport term.
 *
 * The leaf level is by far the largest object in a run (24 B per link: target +
 * flow in each direction), so it is passed around by reference and borrowed
 * rather than copied wherever its owner outlives the reader — see
 * ColumnarTwoLevel::buildFromBorrowedLevel and leaf0()/lvl().
 */
struct ColumnarLevel {
  int n = 0;
  std::vector<double> flow, linkEnter, linkExit;
  std::vector<double> teleFlow, teleWeight;
  std::vector<int> outStart, outTarget;
  std::vector<double> outFlow;
  std::vector<int> inStart, inTarget;
  std::vector<double> inFlow;
};

} // namespace infomap

#endif // COLUMNARLEVEL_H_
