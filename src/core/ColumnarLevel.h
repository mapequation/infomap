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
 * The leaf level is by far the largest object in a run (24 B per link: target +
 * flow in each direction), so it is passed around by reference and borrowed
 * rather than copied wherever its owner outlives the reader — see
 * ColumnarTwoLevel::buildFromBorrowedLevel and leaf0()/lvl().
 */
struct ColumnarLevel {
  int n = 0;
  std::vector<double> flow, enter, exit;
  std::vector<double> teleFlow, teleWeight;
  std::vector<int> outStart, outTarget;
  std::vector<double> outFlow;
  std::vector<int> inStart, inTarget;
  std::vector<double> inFlow;
};

} // namespace infomap

#endif // COLUMNARLEVEL_H_
