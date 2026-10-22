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
 * the base flow model); they sum under aggregation like flow.
 *
 * enter / exit are the unit's TOTAL boundary rates -- the rates the map equation
 * charges: link flow crossing the boundary plus, under recorded teleportation,
 * the teleport enter (T - teleFlow) * teleWeight and exit teleFlow * (1 - teleWeight)
 * (T = the global teleport flow), as FlowCalculator stores them for leaves and as
 * the object-oriented core aggregates them. The teleport part is not additive over
 * members, so a merge or move subtracts the teleport flow between the parts
 * exactly where it subtracts their link flow (ColumnarBoundaryFlow::TeleportSums);
 * only ColumnarBoundaryFlow::aggregate evaluates the closed form. Leaving that
 * subtraction out double-counts teleportation in every module rate (#1042).
 *
 * Under the regularized multilayer prior (#828) the walker teleports within its
 * own layer, so the teleport aggregates are per layer instead: unit i's layers
 * are layerId[layerStart[i] .. layerStart[i + 1]) (compact ids) with
 * that layer's summed teleport flow / weight in layerTeleFlow / layerTeleWeight
 * -- one entry per leaf. All four are empty unless the flow model is layered
 * (ColumnarBoundaryFlow::layered()), and teleFlow then carries nothing (0).
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
  std::vector<int> layerStart, layerId;
  std::vector<double> layerTeleFlow, layerTeleWeight;
  std::vector<int> outStart, outTarget;
  std::vector<double> outFlow;
  std::vector<int> inStart, inTarget;
  std::vector<double> inFlow;
};

} // namespace infomap

#endif // COLUMNARLEVEL_H_
