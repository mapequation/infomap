/*
 * Parallel top-level search benchmark for the atlas equation.
 *
 * Demonstrates the algorithmic consequence of making the index codebook
 * static (see research/atlas-equation/README.md): the objective becomes a sum
 * of per-module terms, so a node move touches exactly two modules and a
 * two-lock commit makes each move's delta exact at commit time. The whole
 * local-moving sweep -- proposal and commit -- then runs in parallel with no
 * global synchronization point.
 *
 * The map equation cannot do this: its index codebook reprices with the total
 * boundary flow q (a plogp(q) term), so every commit writes one global scalar
 * and commits must serialize (Infomap's inner parallelization does exactly
 * that: parallel proposal pass, serial commit pass).
 *
 * Modes benchmarked on a planted-partition graph, Louvain from singletons:
 *   seq-map          sequential local moving, map equation
 *   seq-atlas        sequential local moving, atlas equation
 *   par-map          parallel proposal pass + serial commit pass with exact
 *                    recheck (faithful to Infomap's inner parallelization)
 *   par-atlas        fused parallel sweep: propose and commit under two
 *                    per-module spinlocks, exact delta recheck under lock
 *   par-map-twolock  negative control: the atlas commit strategy applied to
 *                    the map equation (global q read/updated atomically but
 *                    not repriced consistently) -- shows the consistency
 *                    violation as drift between the summed committed deltas
 *                    and the true codelength change
 *
 * Every mode reports: wall time of the top-level sweep phase, total time to
 * convergence (all levels), final two-level codelength recomputed from
 * scratch, and the linearizability check |L_final - (L_init + sum of
 * committed deltas)| for the level-0 sweeps.
 *
 * Build:  g++ -O3 -march=native -fopenmp -std=c++17 -o parallel_bench parallel_bench.cpp
 *         (or `make`, which probes for OpenMP support)
 * Run:    ./parallel_bench [--blocks 200] [--block-size 1000] [--kin 8] [--kout 2]
 *                          [--threads 1,2,4] [--seed 1] [--verbose]
 *         [--sweep-scaling [--warmup-sweeps 2] [--reps 15]]
 *
 * A given seed reproduces bit-for-bit within one toolchain, but not across
 * standard libraries: geometric_distribution and uniform_real_distribution are
 * implementation-defined, so libstdc++ and libc++ generate different graphs
 * from the same seed. Compare numbers only within a single build.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

bool verboseLevels = false;

double plogp(double x) { return x > 1e-300 ? x * std::log2(x) : 0.0; }

// ---------------------------------------------------------------------------
// Graph (undirected, CSR with both directions stored)
// ---------------------------------------------------------------------------

struct Graph {
  unsigned int n = 0;
  std::vector<unsigned int> offset; // n + 1
  std::vector<unsigned int> target;
  std::vector<double> flow; // per directed half-edge: w / (2W)
  std::vector<double> p; // node visit rate = strength / (2W)

  double nodeEntropy() const
  {
    double h = 0.0;
    for (double x : p)
      h -= plogp(x);
    return h;
  }
};

Graph plantedPartition(unsigned int numBlocks, unsigned int blockSize, double kIn, double kOut, unsigned int seed)
{
  const unsigned int n = numBlocks * blockSize;
  // Guard the degenerate shapes: a single block leaves no cross-block pairs
  // (n == blockSize) and a block of one leaves no within-block pairs, so the
  // probabilities would divide by zero and the NaN edge counts below would be
  // cast to unsigned (undefined behavior).
  const double pIn = blockSize > 1 ? kIn / static_cast<double>(blockSize - 1) : 0.0;
  const double pOut = n > blockSize ? kOut / static_cast<double>(n - blockSize) : 0.0;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> unif(0.0, 1.0);

  std::vector<std::pair<unsigned int, unsigned int>> edges;
  edges.reserve(static_cast<size_t>(n) * static_cast<size_t>(kIn + kOut) / 2);
  // geometric_distribution requires p in (0, 1); pIn == 0 means there are no
  // within-block pairs to draw at all.
  std::geometric_distribution<unsigned int> gapIn(pIn > 0.0 ? std::min(pIn, 1.0) : 0.5);
  for (unsigned int u = 0; u < n; ++u) {
    const unsigned int block = u / blockSize;
    const unsigned int blockEnd = (block + 1) * blockSize;
    // within-block partners v > u via geometric gap skipping
    if (pIn > 0.0) {
      unsigned int v = u + 1 + gapIn(rng);
      while (v < blockEnd) {
        edges.emplace_back(u, v);
        v += 1 + gapIn(rng);
      }
    }
    // cross-block partners: expected count, uniform targets outside the block
    const double expected = pOut * static_cast<double>(n - blockSize);
    auto count = static_cast<unsigned int>(expected);
    if (unif(rng) < expected - count)
      ++count;
    for (unsigned int t = 0; t < count; ++t) {
      unsigned int w = rng() % n;
      if (w / blockSize != block) {
        edges.emplace_back(std::min(u, w), std::max(u, w));
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

  Graph g;
  g.n = n;
  std::vector<unsigned int> degree(n, 0);
  for (auto& e : edges) {
    ++degree[e.first];
    ++degree[e.second];
  }
  g.offset.assign(n + 1, 0);
  for (unsigned int u = 0; u < n; ++u)
    g.offset[u + 1] = g.offset[u] + degree[u];
  g.target.resize(g.offset[n]);
  g.flow.resize(g.offset[n]);
  const double totalWeight = static_cast<double>(edges.size());
  const double linkFlow = 1.0 / (2.0 * totalWeight);
  std::vector<unsigned int> cursor(g.offset.begin(), g.offset.end() - 1);
  for (auto& e : edges) {
    g.target[cursor[e.first]] = e.second;
    g.flow[cursor[e.first]++] = linkFlow;
    g.target[cursor[e.second]] = e.first;
    g.flow[cursor[e.second]++] = linkFlow;
  }
  g.p.assign(n, 0.0);
  for (unsigned int u = 0; u < n; ++u) {
    double s = 0.0;
    for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
      s += g.flow[k];
    g.p[u] = s;
  }
  return g;
}

// ---------------------------------------------------------------------------
// Objectives on per-module aggregates (undirected: Qin == Qout == Q)
// ---------------------------------------------------------------------------

enum class Objective { MapEq, Atlas };

// Module term of the atlas equation: static address cost + module codebook.
double gAtlas(double P, double Q)
{
  const double idx = (Q > 1e-300 && P > 1e-300) ? -Q * std::log2(P) : 0.0;
  return idx + plogp(P + Q) - plogp(Q);
}

struct ModuleState {
  std::vector<std::atomic<double>> P;
  std::vector<std::atomic<double>> Q;
  std::vector<std::atomic<unsigned int>> members;
  std::vector<std::atomic_flag> lock;
  std::atomic<double> qTotal { 0.0 }; // used by the map equation only

  explicit ModuleState(unsigned int m) : P(m), Q(m), members(m), lock(m)
  {
    for (unsigned int i = 0; i < m; ++i) {
      P[i].store(0.0, std::memory_order_relaxed);
      Q[i].store(0.0, std::memory_order_relaxed);
      members[i].store(0, std::memory_order_relaxed);
      lock[i].clear();
    }
  }

  void acquire(unsigned int i)
  {
    while (lock[i].test_and_set(std::memory_order_acquire)) {
    }
  }
  void release(unsigned int i) { lock[i].clear(std::memory_order_release); }
};

double codelength(Objective obj, const std::vector<double>& P, const std::vector<double>& Q, double nodeEntropy)
{
  double L = nodeEntropy;
  double q = 0.0;
  for (size_t i = 0; i < P.size(); ++i) {
    if (P[i] <= 0.0 && Q[i] <= 0.0)
      continue;
    L += plogp(P[i] + Q[i]) - plogp(Q[i]);
    if (obj == Objective::Atlas) {
      L += (Q[i] > 1e-300 && P[i] > 1e-300) ? -Q[i] * std::log2(P[i]) : 0.0;
    } else {
      L -= plogp(Q[i]);
      q += Q[i];
    }
  }
  if (obj == Objective::MapEq)
    L += plogp(q);
  return L;
}

// ---------------------------------------------------------------------------
// Local moving sweeps
// ---------------------------------------------------------------------------

struct SweepStats {
  unsigned int moved = 0;
  unsigned int fastAccepts = 0; // serial-commit mode: proposals taken in O(1)
  double seconds = 0.0;
  double committedDelta = 0.0; // sum of the deltas believed at commit time
};

using Clock = std::chrono::steady_clock;

double seconds(Clock::time_point a, Clock::time_point b)
{
  return std::chrono::duration<double>(b - a).count();
}

struct LinkSums {
  // flow between the node and each adjacent module, gathered per node visit
  std::vector<double> sum;
  std::vector<unsigned int> touched;
  double total = 0.0; // the node's total incident link flow (wNode)
  explicit LinkSums(unsigned int m) : sum(m, 0.0) { touched.reserve(64); }
  void clear()
  {
    for (unsigned int t : touched)
      sum[t] = 0.0;
    touched.clear();
    total = 0.0;
  }
  void add(unsigned int module, double f)
  {
    if (sum[module] == 0.0)
      touched.push_back(module);
    sum[module] += f;
    total += f;
  }
};

// Aggregates of the two touched modules after moving a node with visit rate
// pNode, total incident link flow wNode, and boundary sums kOld/kNew
// (undirected). At the leaf level wNode == pNode, but a coarse supernode
// carries intra-supernode flow in pNode that never appears on links, so the
// boundary update must use wNode.
struct TwoModulesAfter {
  double pOld, qOld, pNew, qNew;
};

TwoModulesAfter after(double POld, double QOld, double PNew, double QNew, double pNode, double wNode, double kOld, double kNew)
{
  return { POld - pNode, QOld - wNode + 2.0 * kOld, PNew + pNode, QNew + wNode - 2.0 * kNew };
}

double deltaAtlas(double POld, double QOld, double PNew, double QNew, double pNode, double wNode, double kOld, double kNew)
{
  const auto a = after(POld, QOld, PNew, QNew, pNode, wNode, kOld, kNew);
  return gAtlas(a.pOld, a.qOld) + gAtlas(a.pNew, a.qNew) - gAtlas(POld, QOld) - gAtlas(PNew, QNew);
}

// The map equation delta splits cleanly into a part that depends only on the
// two endpoint modules and a single global repricing of plogp(q). Keeping the
// two apart is what lets the serial commit pass below fast-accept a proposal
// in O(1) and still be exact: if neither endpoint module has been touched,
// `perModule` is unchanged from the proposal snapshot and only the global part
// needs the live q.
struct MapDeltaParts {
  double perModule;
  double dq;
};

MapDeltaParts deltaMapParts(double POld, double QOld, double PNew, double QNew, double pNode, double wNode, double kOld, double kNew)
{
  const auto a = after(POld, QOld, PNew, QNew, pNode, wNode, kOld, kNew);
  auto g = [](double P, double Q) { return -plogp(Q) + plogp(P + Q) - plogp(Q); };
  return { g(a.pOld, a.qOld) + g(a.pNew, a.qNew) - g(POld, QOld) - g(PNew, QNew),
           -QOld - QNew + a.qOld + a.qNew };
}

double deltaMapFromParts(const MapDeltaParts& parts, double qTotal)
{
  return parts.perModule + plogp(qTotal + parts.dq) - plogp(qTotal);
}

double deltaMap(double POld, double QOld, double PNew, double QNew, double pNode, double wNode, double kOld, double kNew, double qTotal)
{
  return deltaMapFromParts(deltaMapParts(POld, QOld, PNew, QNew, pNode, wNode, kOld, kNew), qTotal);
}

struct Level {
  const Graph* graph;
  std::vector<std::atomic<unsigned int>> module;
  std::vector<std::atomic<char>> dirty;
  ModuleState state;
  // Serial-commit fast-accept stamps (see sweepParallelMapSerialCommit).
  std::vector<unsigned int> touchedStamp;
  unsigned int commitStamp = 0;

  explicit Level(const Graph& g) : graph(&g), module(g.n), dirty(g.n), state(g.n), touchedStamp(g.n, 0)
  {
    for (unsigned int u = 0; u < g.n; ++u)
      dirty[u].store(1, std::memory_order_relaxed);
    double q = 0.0;
    for (unsigned int u = 0; u < g.n; ++u) {
      module[u].store(u, std::memory_order_relaxed);
      state.P[u].store(g.p[u], std::memory_order_relaxed);
      state.members[u].store(1, std::memory_order_relaxed);
      double boundary = 0.0;
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        boundary += g.flow[k];
      state.Q[u].store(boundary, std::memory_order_relaxed);
      q += boundary;
    }
    state.qTotal.store(q, std::memory_order_relaxed);
  }

  void snapshot(std::vector<double>& P, std::vector<double>& Q) const
  {
    P.resize(graph->n);
    Q.resize(graph->n);
    for (unsigned int i = 0; i < graph->n; ++i) {
      P[i] = state.P[i].load(std::memory_order_relaxed);
      Q[i] = state.Q[i].load(std::memory_order_relaxed);
    }
  }

  double currentCodelength(Objective obj, double nodeEntropy) const
  {
    std::vector<double> P, Q;
    snapshot(P, Q);
    return codelength(obj, P, Q, nodeEntropy);
  }
};

constexpr double kMinImprovement = 1e-10;

// --- sequential sweep -------------------------------------------------------

SweepStats sweepSequential(Level& level, Objective obj, std::vector<unsigned int>& order, LinkSums& sums)
{
  const Graph& g = *level.graph;
  SweepStats stats;
  auto t0 = Clock::now();
  for (unsigned int u : order) {
    if (!level.dirty[u].load(std::memory_order_relaxed))
      continue;
    sums.clear();
    for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
      sums.add(level.module[g.target[k]].load(std::memory_order_relaxed), g.flow[k]);
    const unsigned int oldModule = level.module[u].load(std::memory_order_relaxed);
    const double pNode = g.p[u];
    const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
    const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
    const double kOld = sums.sum[oldModule];
    const double qTotal = level.state.qTotal.load(std::memory_order_relaxed);

    unsigned int best = oldModule;
    double bestDelta = -kMinImprovement;
    double bestK = 0.0;
    for (unsigned int cand : sums.touched) {
      if (cand == oldModule)
        continue;
      const double PNew = level.state.P[cand].load(std::memory_order_relaxed);
      const double QNew = level.state.Q[cand].load(std::memory_order_relaxed);
      const double kNew = sums.sum[cand];
      const double d = obj == Objective::Atlas
          ? deltaAtlas(POld, QOld, PNew, QNew, pNode, sums.total, kOld, kNew)
          : deltaMap(POld, QOld, PNew, QNew, pNode, sums.total, kOld, kNew, qTotal);
      if (d < bestDelta) {
        bestDelta = d;
        best = cand;
        bestK = kNew;
      }
    }
    if (best != oldModule) {
      const double PNew = level.state.P[best].load(std::memory_order_relaxed);
      const double QNew = level.state.Q[best].load(std::memory_order_relaxed);
      const auto a = after(POld, QOld, PNew, QNew, pNode, sums.total, kOld, bestK);
      level.state.P[oldModule].store(a.pOld, std::memory_order_relaxed);
      level.state.Q[oldModule].store(a.qOld, std::memory_order_relaxed);
      level.state.P[best].store(a.pNew, std::memory_order_relaxed);
      level.state.Q[best].store(a.qNew, std::memory_order_relaxed);
      level.state.members[oldModule].fetch_sub(1, std::memory_order_relaxed);
      level.state.members[best].fetch_add(1, std::memory_order_relaxed);
      level.state.qTotal.store(qTotal - QOld - QNew + a.qOld + a.qNew, std::memory_order_relaxed);
      level.module[u].store(best, std::memory_order_relaxed);
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        level.dirty[g.target[k]].store(1, std::memory_order_relaxed);
      stats.committedDelta += bestDelta;
      ++stats.moved;
    } else {
      level.dirty[u].store(0, std::memory_order_relaxed);
    }
  }
  stats.seconds = seconds(t0, Clock::now());
  return stats;
}

// --- fused parallel sweep for the atlas equation (two-lock commits) ---------

SweepStats sweepParallelAtlas(Level& level, const std::vector<unsigned int>& order)
{
  const Graph& g = *level.graph;
  SweepStats stats;
  std::atomic<unsigned int> moved { 0 };
  double committed = 0.0;
  auto t0 = Clock::now();

#ifdef _OPENMP
#pragma omp parallel reduction(+ : committed)
#endif
  {
    LinkSums sums(g.n);
#ifdef _OPENMP
#pragma omp for schedule(dynamic, 256)
#endif
    for (long long idx = 0; idx < static_cast<long long>(order.size()); ++idx) {
      const unsigned int u = order[static_cast<size_t>(idx)];
      if (!level.dirty[u].load(std::memory_order_relaxed))
        continue;
      // Claim before scanning: a concurrent commit that re-dirties u after
      // this store is not lost, it just schedules a revisit next sweep.
      level.dirty[u].store(0, std::memory_order_relaxed);
      sums.clear();
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        sums.add(level.module[g.target[k]].load(std::memory_order_relaxed), g.flow[k]);
      const unsigned int oldModule = level.module[u].load(std::memory_order_relaxed);
      const double pNode = g.p[u];
      const double kOld = sums.sum[oldModule];

      // Unlocked scan: aggregates may be stale; used only to pick a candidate.
      unsigned int best = oldModule;
      double bestDelta = -kMinImprovement;
      {
        const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
        const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
        for (unsigned int cand : sums.touched) {
          if (cand == oldModule)
            continue;
          const double d = deltaAtlas(POld, QOld,
                                      level.state.P[cand].load(std::memory_order_relaxed),
                                      level.state.Q[cand].load(std::memory_order_relaxed),
                                      pNode, sums.total, kOld, sums.sum[cand]);
          if (d < bestDelta) {
            bestDelta = d;
            best = cand;
          }
        }
      }
      if (best == oldModule)
        continue;

      // Ordered two-lock commit with exact recheck. Membership of the two
      // locked modules cannot change while we hold both locks, and moves
      // among other modules cannot affect this delta (locality theorem), so
      // the recheck below is exact -- this is what the map equation lacks.
      const unsigned int lockA = std::min(oldModule, best);
      const unsigned int lockB = std::max(oldModule, best);
      level.state.acquire(lockA);
      level.state.acquire(lockB);

      const unsigned int curModule = level.module[u].load(std::memory_order_relaxed);
      if (curModule != oldModule) { // someone moved us? (only this thread owns u; defensive)
        level.state.release(lockB);
        level.state.release(lockA);
        continue;
      }
      // Re-gather link sums to the two locked modules (neighbor module ids of
      // third modules may be stale, but any value != oldModule/best classifies
      // identically for this delta).
      double kOldExact = 0.0;
      double kNewExact = 0.0;
      double wNode = 0.0;
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k) {
        const unsigned int m = level.module[g.target[k]].load(std::memory_order_relaxed);
        wNode += g.flow[k];
        if (m == oldModule)
          kOldExact += g.flow[k];
        else if (m == best)
          kNewExact += g.flow[k];
      }
      const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
      const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
      const double PNew = level.state.P[best].load(std::memory_order_relaxed);
      const double QNew = level.state.Q[best].load(std::memory_order_relaxed);
      const double d = deltaAtlas(POld, QOld, PNew, QNew, pNode, wNode, kOldExact, kNewExact);
      bool didMove = false;
      if (d < -kMinImprovement) {
        const auto a = after(POld, QOld, PNew, QNew, pNode, wNode, kOldExact, kNewExact);
        level.state.P[oldModule].store(a.pOld, std::memory_order_relaxed);
        level.state.Q[oldModule].store(a.qOld, std::memory_order_relaxed);
        level.state.P[best].store(a.pNew, std::memory_order_relaxed);
        level.state.Q[best].store(a.qNew, std::memory_order_relaxed);
        level.state.members[oldModule].fetch_sub(1, std::memory_order_relaxed);
        level.state.members[best].fetch_add(1, std::memory_order_relaxed);
        level.module[u].store(best, std::memory_order_relaxed);
        committed += d;
        moved.fetch_add(1, std::memory_order_relaxed);
        didMove = true;
      }
      level.state.release(lockB);
      level.state.release(lockA);
      if (didMove) {
        level.dirty[u].store(1, std::memory_order_relaxed); // mover stays dirty
        for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
          level.dirty[g.target[k]].store(1, std::memory_order_relaxed);
      } else if (best != oldModule) {
        level.dirty[u].store(1, std::memory_order_relaxed); // recheck rejected: retry
      }
    }
  }
  stats.moved = moved.load();
  stats.committedDelta = committed;
  stats.seconds = seconds(t0, Clock::now());
  return stats;
}

// --- parallel proposal + serial commit for the map equation -----------------
// Faithful to InfomapOptimizer::tryMoveEachNodeIntoBestModuleInParallel:
// proposals against a sweep-start snapshot, then a serial commit pass that
// rechecks against the live state (the global q makes parallel commits
// unsound, so the commit pass cannot be parallelized).

struct Proposal {
  unsigned int node = 0;
  unsigned int oldModule = 0;
  unsigned int newModule = 0;
  bool valid = false;
  double wNode = 0.0;
  double kOld = 0.0;
  double kNew = 0.0;
  MapDeltaParts parts {};
};

SweepStats sweepParallelMapSerialCommit(Level& level, const std::vector<unsigned int>& order, double* commitSeconds)
{
  const Graph& g = *level.graph;
  SweepStats stats;
  auto t0 = Clock::now();

  // No snapshot copies: the proposal pass runs entirely before the commit
  // pass, so reading the live state here yields the sweep-start values anyway
  // (this is also what Infomap does -- it proposes against live
  // m_moduleFlowData). Copying 3 arrays per sweep would be pure overhead
  // charged to this mode.
  const double qTotal = level.state.qTotal.load(std::memory_order_relaxed);
  auto moduleOf = [&level](unsigned int x) { return level.module[x].load(std::memory_order_relaxed); };
  auto Pof = [&level](unsigned int m) { return level.state.P[m].load(std::memory_order_relaxed); };
  auto Qof = [&level](unsigned int m) { return level.state.Q[m].load(std::memory_order_relaxed); };

  std::vector<Proposal> proposals(order.size());

#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    LinkSums sums(g.n);
#ifdef _OPENMP
#pragma omp for schedule(dynamic, 256)
#endif
    for (long long idx = 0; idx < static_cast<long long>(order.size()); ++idx) {
      const unsigned int u = order[static_cast<size_t>(idx)];
      if (!level.dirty[u].load(std::memory_order_relaxed))
        continue;
      sums.clear();
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        sums.add(moduleOf(g.target[k]), g.flow[k]);
      const unsigned int oldModule = moduleOf(u);
      const double pNode = g.p[u];
      const double kOld = sums.sum[oldModule];
      unsigned int best = oldModule;
      double bestDelta = -kMinImprovement;
      for (unsigned int cand : sums.touched) {
        if (cand == oldModule)
          continue;
        const double d = deltaMap(Pof(oldModule), Qof(oldModule), Pof(cand), Qof(cand), pNode, sums.total, kOld, sums.sum[cand], qTotal);
        if (d < bestDelta) {
          bestDelta = d;
          best = cand;
        }
      }
      if (best != oldModule) {
        // Carry the link sums and the module-local part of the delta so the
        // commit pass can fast-accept without re-walking the node's edges.
        const auto parts = deltaMapParts(Pof(oldModule), Qof(oldModule), Pof(best), Qof(best), pNode, sums.total, kOld, sums.sum[best]);
        proposals[static_cast<size_t>(idx)] = Proposal { u, oldModule, best, true, sums.total, kOld, sums.sum[best], parts };
      } else {
        // Proposal phase is a barrier-separated snapshot: no concurrent
        // commits can re-dirty u here, so clearing is race-free.
        level.dirty[u].store(0, std::memory_order_relaxed);
      }
    }
  }

  auto tCommit = Clock::now();
  // Serial commit pass. Mirrors Infomap's structure
  // (src/core/InfomapOptimizer.h:887-909): proposals whose two endpoint
  // modules are both still untouched in this pass are fast-accepted from the
  // snapshot; the rest re-walk the node's edges and recompute the delta.
  ++level.commitStamp;
  const unsigned int commitStamp = level.commitStamp;
  if (level.touchedStamp.size() < g.n)
    level.touchedStamp.assign(g.n, 0);
  for (const Proposal& prop : proposals) {
    if (!prop.valid)
      continue;
    const unsigned int u = prop.node;
    const unsigned int oldModule = level.module[u].load(std::memory_order_relaxed);
    if (oldModule != prop.oldModule)
      continue;
    const unsigned int best = prop.newModule;

    if (level.touchedStamp[oldModule] != commitStamp && level.touchedStamp[best] != commitStamp) {
      // Neither module has changed, so the module-local part of the delta is
      // still exact and only the global term needs the live q -- O(1), and no
      // edge walk. (Infomap fast-accepts on the snapshot delta outright; the
      // O(1) exact repricing here keeps this benchmark's consistency metric
      // meaningful and only makes the map equation look better, never worse.)
      const double qLive = level.state.qTotal.load(std::memory_order_relaxed);
      const double d = deltaMapFromParts(prop.parts, qLive);
      if (d >= -kMinImprovement)
        continue;
      const double POldF = level.state.P[oldModule].load(std::memory_order_relaxed);
      const double QOldF = level.state.Q[oldModule].load(std::memory_order_relaxed);
      const double PNewF = level.state.P[best].load(std::memory_order_relaxed);
      const double QNewF = level.state.Q[best].load(std::memory_order_relaxed);
      const auto a = after(POldF, QOldF, PNewF, QNewF, g.p[u], prop.wNode, prop.kOld, prop.kNew);
      level.state.P[oldModule].store(a.pOld, std::memory_order_relaxed);
      level.state.Q[oldModule].store(a.qOld, std::memory_order_relaxed);
      level.state.P[best].store(a.pNew, std::memory_order_relaxed);
      level.state.Q[best].store(a.qNew, std::memory_order_relaxed);
      level.state.members[oldModule].fetch_sub(1, std::memory_order_relaxed);
      level.state.members[best].fetch_add(1, std::memory_order_relaxed);
      level.state.qTotal.store(qLive - QOldF - QNewF + a.qOld + a.qNew, std::memory_order_relaxed);
      level.module[u].store(best, std::memory_order_relaxed);
      level.touchedStamp[oldModule] = commitStamp;
      level.touchedStamp[best] = commitStamp;
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        level.dirty[g.target[k]].store(1, std::memory_order_relaxed);
      stats.committedDelta += d;
      ++stats.moved;
      ++stats.fastAccepts;
      continue;
    }

    double kOld = 0.0;
    double kNew = 0.0;
    double wNode = 0.0;
    for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k) {
      const unsigned int m = level.module[g.target[k]].load(std::memory_order_relaxed);
      wNode += g.flow[k];
      if (m == oldModule)
        kOld += g.flow[k];
      else if (m == best)
        kNew += g.flow[k];
    }
    const double pNode = g.p[u];
    const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
    const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
    const double PNew = level.state.P[best].load(std::memory_order_relaxed);
    const double QNew = level.state.Q[best].load(std::memory_order_relaxed);
    const double qLive = level.state.qTotal.load(std::memory_order_relaxed);
    const double d = deltaMap(POld, QOld, PNew, QNew, pNode, wNode, kOld, kNew, qLive);
    if (d >= -kMinImprovement)
      continue;
    const auto a = after(POld, QOld, PNew, QNew, pNode, wNode, kOld, kNew);
    level.state.P[oldModule].store(a.pOld, std::memory_order_relaxed);
    level.state.Q[oldModule].store(a.qOld, std::memory_order_relaxed);
    level.state.P[best].store(a.pNew, std::memory_order_relaxed);
    level.state.Q[best].store(a.qNew, std::memory_order_relaxed);
    level.state.members[oldModule].fetch_sub(1, std::memory_order_relaxed);
    level.state.members[best].fetch_add(1, std::memory_order_relaxed);
    level.state.qTotal.store(qLive - QOld - QNew + a.qOld + a.qNew, std::memory_order_relaxed);
    level.module[u].store(best, std::memory_order_relaxed);
    level.touchedStamp[oldModule] = commitStamp;
    level.touchedStamp[best] = commitStamp;
    for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
      level.dirty[g.target[k]].store(1, std::memory_order_relaxed);
    stats.committedDelta += d;
    ++stats.moved;
  }
  auto t1 = Clock::now();
  if (commitSeconds != nullptr)
    *commitSeconds += seconds(tCommit, t1);
  stats.seconds = seconds(t0, t1);
  return stats;
}

// --- negative control: two-lock commits under the map equation --------------
// Identical locking discipline to sweepParallelAtlas, but the delta depends on
// the global q, which other threads mutate concurrently. The committed deltas
// are therefore not the true codelength changes; the run reports the drift.

SweepStats sweepParallelMapTwoLock(Level& level, const std::vector<unsigned int>& order)
{
  const Graph& g = *level.graph;
  SweepStats stats;
  std::atomic<unsigned int> moved { 0 };
  double committed = 0.0;
  auto t0 = Clock::now();

#ifdef _OPENMP
#pragma omp parallel reduction(+ : committed)
#endif
  {
    LinkSums sums(g.n);
#ifdef _OPENMP
#pragma omp for schedule(dynamic, 256)
#endif
    for (long long idx = 0; idx < static_cast<long long>(order.size()); ++idx) {
      const unsigned int u = order[static_cast<size_t>(idx)];
      if (!level.dirty[u].load(std::memory_order_relaxed))
        continue;
      level.dirty[u].store(0, std::memory_order_relaxed);
      sums.clear();
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
        sums.add(level.module[g.target[k]].load(std::memory_order_relaxed), g.flow[k]);
      const unsigned int oldModule = level.module[u].load(std::memory_order_relaxed);
      const double pNode = g.p[u];
      const double kOld = sums.sum[oldModule];
      unsigned int best = oldModule;
      double bestDelta = -kMinImprovement;
      {
        const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
        const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
        const double qTotal = level.state.qTotal.load(std::memory_order_relaxed);
        for (unsigned int cand : sums.touched) {
          if (cand == oldModule)
            continue;
          const double d = deltaMap(POld, QOld,
                                    level.state.P[cand].load(std::memory_order_relaxed),
                                    level.state.Q[cand].load(std::memory_order_relaxed),
                                    pNode, sums.total, kOld, sums.sum[cand], qTotal);
          if (d < bestDelta) {
            bestDelta = d;
            best = cand;
          }
        }
      }
      if (best == oldModule)
        continue;

      const unsigned int lockA = std::min(oldModule, best);
      const unsigned int lockB = std::max(oldModule, best);
      level.state.acquire(lockA);
      level.state.acquire(lockB);
      double kOldExact = 0.0;
      double kNewExact = 0.0;
      double wNode = 0.0;
      for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k) {
        const unsigned int m = level.module[g.target[k]].load(std::memory_order_relaxed);
        wNode += g.flow[k];
        if (m == oldModule)
          kOldExact += g.flow[k];
        else if (m == best)
          kNewExact += g.flow[k];
      }
      const double POld = level.state.P[oldModule].load(std::memory_order_relaxed);
      const double QOld = level.state.Q[oldModule].load(std::memory_order_relaxed);
      const double PNew = level.state.P[best].load(std::memory_order_relaxed);
      const double QNew = level.state.Q[best].load(std::memory_order_relaxed);
      // q is read without any lock that protects it: this is the unsound step.
      const double qStale = level.state.qTotal.load(std::memory_order_relaxed);
      const double d = deltaMap(POld, QOld, PNew, QNew, pNode, wNode, kOldExact, kNewExact, qStale);
      bool didMove = false;
      if (d < -kMinImprovement) {
        const auto a = after(POld, QOld, PNew, QNew, pNode, wNode, kOldExact, kNewExact);
        level.state.P[oldModule].store(a.pOld, std::memory_order_relaxed);
        level.state.Q[oldModule].store(a.qOld, std::memory_order_relaxed);
        level.state.P[best].store(a.pNew, std::memory_order_relaxed);
        level.state.Q[best].store(a.qNew, std::memory_order_relaxed);
        level.state.members[oldModule].fetch_sub(1, std::memory_order_relaxed);
        level.state.members[best].fetch_add(1, std::memory_order_relaxed);
        // atomic read-modify-write of the global (fetch_add on atomic<double>)
        double expected = level.state.qTotal.load(std::memory_order_relaxed);
        const double dq = -QOld - QNew + a.qOld + a.qNew;
        while (!level.state.qTotal.compare_exchange_weak(expected, expected + dq, std::memory_order_relaxed)) {
        }
        level.module[u].store(best, std::memory_order_relaxed);
        committed += d;
        moved.fetch_add(1, std::memory_order_relaxed);
        didMove = true;
      }
      level.state.release(lockB);
      level.state.release(lockA);
      if (didMove) {
        level.dirty[u].store(1, std::memory_order_relaxed);
        for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k)
          level.dirty[g.target[k]].store(1, std::memory_order_relaxed);
      } else if (best != oldModule) {
        level.dirty[u].store(1, std::memory_order_relaxed);
      }
    }
  }
  stats.moved = moved.load();
  stats.committedDelta = committed;
  stats.seconds = seconds(t0, Clock::now());
  return stats;
}

// ---------------------------------------------------------------------------
// Aggregation (module graph) for full Louvain
// ---------------------------------------------------------------------------

Graph aggregate(const Level& level, std::vector<unsigned int>& leafToCoarse)
{
  const Graph& g = *level.graph;
  std::vector<unsigned int> remap(g.n, 0);
  unsigned int numModules = 0;
  std::vector<unsigned int> moduleOf(g.n);
  for (unsigned int u = 0; u < g.n; ++u)
    moduleOf[u] = level.module[u].load(std::memory_order_relaxed);
  {
    std::vector<char> seen(g.n, 0);
    for (unsigned int u = 0; u < g.n; ++u) {
      if (!seen[moduleOf[u]]) {
        seen[moduleOf[u]] = 1;
        remap[moduleOf[u]] = numModules++;
      }
    }
  }
  for (unsigned int u = 0; u < g.n; ++u)
    leafToCoarse[u] = remap[moduleOf[u]];

  Graph coarse;
  coarse.n = numModules;
  coarse.p.assign(numModules, 0.0);
  for (unsigned int u = 0; u < g.n; ++u)
    coarse.p[remap[moduleOf[u]]] += g.p[u];

  std::unordered_map<uint64_t, double> flows;
  flows.reserve(g.target.size() / 4);
  for (unsigned int u = 0; u < g.n; ++u) {
    const unsigned int mu = remap[moduleOf[u]];
    for (unsigned int k = g.offset[u]; k < g.offset[u + 1]; ++k) {
      const unsigned int mv = remap[moduleOf[g.target[k]]];
      if (mu != mv)
        flows[(static_cast<uint64_t>(mu) << 32) | mv] += g.flow[k];
    }
  }
  std::vector<unsigned int> degree(numModules, 0);
  for (auto& kv : flows)
    ++degree[kv.first >> 32];
  coarse.offset.assign(numModules + 1, 0);
  for (unsigned int m = 0; m < numModules; ++m)
    coarse.offset[m + 1] = coarse.offset[m] + degree[m];
  coarse.target.resize(coarse.offset[numModules]);
  coarse.flow.resize(coarse.offset[numModules]);
  std::vector<unsigned int> cursor(coarse.offset.begin(), coarse.offset.end() - 1);
  for (auto& kv : flows) {
    const auto mu = static_cast<unsigned int>(kv.first >> 32);
    const auto mv = static_cast<unsigned int>(kv.first & 0xffffffffu);
    coarse.target[cursor[mu]] = mv;
    coarse.flow[cursor[mu]++] = kv.second;
  }
  return coarse;
}

// ---------------------------------------------------------------------------
// Full runs
// ---------------------------------------------------------------------------

enum class Mode { SeqMap, SeqAtlas, ParMapSerialCommit, ParAtlas, ParMapTwoLock };

const char* modeName(Mode m)
{
  switch (m) {
  case Mode::SeqMap:
    return "seq-map";
  case Mode::SeqAtlas:
    return "seq-atlas";
  case Mode::ParMapSerialCommit:
    return "par-map (serial commit)";
  case Mode::ParAtlas:
    return "par-atlas (two-lock)";
  case Mode::ParMapTwoLock:
    return "par-map-twolock (unsound)";
  }
  return "?";
}

Objective objectiveOf(Mode m)
{
  return (m == Mode::SeqAtlas || m == Mode::ParAtlas) ? Objective::Atlas : Objective::MapEq;
}

// ---------------------------------------------------------------------------
// Controlled strong-scaling measurement
// ---------------------------------------------------------------------------
//
// End-to-end phase times conflate two things: how fast a sweep runs, and how
// many sweeps the search needs (concurrency changes the trajectory, so thread
// count alters both). This measures only the first: advance to a fixed state,
// snapshot it, then run ONE sweep from that identical state at each thread
// count, repeated, keeping the minimum. The minimum is the right estimator
// here because timing noise on a shared machine is one-sided -- interruptions
// only ever add time.

struct LevelSnapshot {
  std::vector<unsigned int> module;
  std::vector<char> dirty;
  std::vector<double> P, Q;
  std::vector<unsigned int> members;
  std::vector<unsigned int> touchedStamp;
  double qTotal = 0.0;
};

LevelSnapshot saveLevel(const Level& level)
{
  const unsigned int n = level.graph->n;
  LevelSnapshot s;
  s.module.resize(n);
  s.dirty.resize(n);
  s.P.resize(n);
  s.Q.resize(n);
  s.members.resize(n);
  for (unsigned int i = 0; i < n; ++i) {
    s.module[i] = level.module[i].load(std::memory_order_relaxed);
    s.dirty[i] = level.dirty[i].load(std::memory_order_relaxed);
    s.P[i] = level.state.P[i].load(std::memory_order_relaxed);
    s.Q[i] = level.state.Q[i].load(std::memory_order_relaxed);
    s.members[i] = level.state.members[i].load(std::memory_order_relaxed);
  }
  s.touchedStamp = level.touchedStamp;
  s.qTotal = level.state.qTotal.load(std::memory_order_relaxed);
  return s;
}

void restoreLevel(Level& level, const LevelSnapshot& s)
{
  const unsigned int n = level.graph->n;
  for (unsigned int i = 0; i < n; ++i) {
    level.module[i].store(s.module[i], std::memory_order_relaxed);
    level.dirty[i].store(s.dirty[i], std::memory_order_relaxed);
    level.state.P[i].store(s.P[i], std::memory_order_relaxed);
    level.state.Q[i].store(s.Q[i], std::memory_order_relaxed);
    level.state.members[i].store(s.members[i], std::memory_order_relaxed);
  }
  level.touchedStamp = s.touchedStamp;
  level.state.qTotal.store(s.qTotal, std::memory_order_relaxed);
}

void sweepScaling(const Graph& g, const std::vector<int>& threads, unsigned int warmupSweeps, unsigned int reps, unsigned int seed)
{
  std::printf("\nControlled strong scaling: one sweep from an identical state, best of %u\n", reps);
  std::printf("(state = %u sequential sweeps from singletons; work is identical across rows)\n\n", warmupSweeps);
  std::printf("%-27s %8s | %10s %10s | %9s %6s | %8s\n",
              "mode", "threads", "sweep", "vs 1 thread", "serial", "share", "moves");

  for (Mode mode : { Mode::ParAtlas, Mode::ParMapSerialCommit }) {
    const Objective obj = objectiveOf(mode);
    Level level(g);
    std::vector<unsigned int> order(g.n);
    for (unsigned int u = 0; u < g.n; ++u)
      order[u] = u;
    std::mt19937 rng(seed);
    std::shuffle(order.begin(), order.end(), rng);
    {
      LinkSums warmSums(g.n);
      for (unsigned int k = 0; k < warmupSweeps; ++k)
        sweepSequential(level, obj, order, warmSums);
    }
    const LevelSnapshot snap = saveLevel(level);

    double baseline = 0.0;
    for (int t : threads) {
#ifdef _OPENMP
      omp_set_num_threads(t);
#endif
      double best = 1e300;
      double bestCommit = 0.0;
      unsigned int moves = 0;
      for (unsigned int r = 0; r < reps; ++r) {
        restoreLevel(level, snap);
        double commit = 0.0;
        const SweepStats st = mode == Mode::ParAtlas
            ? sweepParallelAtlas(level, order)
            : sweepParallelMapSerialCommit(level, order, &commit);
        if (st.seconds < best) {
          best = st.seconds;
          bestCommit = commit;
          moves = st.moved;
        }
      }
      if (baseline == 0.0)
        baseline = best;
      std::printf("%-27s %8d | %9.4fs %10.2fx | %8.4fs %6.1f%% | %8u\n",
                  modeName(mode), t, best, baseline / best, bestCommit, 100.0 * bestCommit / best, moves);
    }
  }
}

struct RunResult {
  double level0Seconds = 0.0;
  double level0CommitSeconds = 0.0;
  double totalSeconds = 0.0;
  double finalMap = 0.0;
  double finalAtlas = 0.0;
  double consistency = 0.0; // |L_after - (L_before + sum committed deltas)| at level 0
  unsigned int level0Sweeps = 0;
  unsigned int modules = 0;
  unsigned int level0Moves = 0;
  unsigned int level0FastAccepts = 0;
};

RunResult run(const Graph& leafGraph, Mode mode, unsigned int seed, unsigned int maxSweeps)
{
  const Objective obj = objectiveOf(mode);
  const double leafEntropy = leafGraph.nodeEntropy();
  RunResult result;
  auto tRun = Clock::now();

  std::vector<unsigned int> leafAssignment(leafGraph.n);
  for (unsigned int u = 0; u < leafGraph.n; ++u)
    leafAssignment[u] = u;

  const Graph* current = &leafGraph;
  Graph owned; // only the current coarse level is kept
  std::mt19937 rng(seed);
  bool topLevel = true;
  unsigned int levelIndex = 0;

  while (true) {
    Level level(*current);
    std::vector<unsigned int> order(current->n);
    for (unsigned int u = 0; u < current->n; ++u)
      order[u] = u;

    const double before = level.currentCodelength(obj, 0.0);
    double committed = 0.0;
    unsigned int sweeps = 0;
    double levelSeconds = 0.0;
    double commitSeconds = 0.0;
    unsigned int levelMoves = 0;
    unsigned int levelFastAccepts = 0;
    LinkSums seqSums(current->n);
    bool verified = false;
    for (; sweeps < maxSweeps; ++sweeps) {
      std::shuffle(order.begin(), order.end(), rng);
      SweepStats stats;
      switch (mode) {
      case Mode::SeqMap:
      case Mode::SeqAtlas:
        stats = sweepSequential(level, obj, order, seqSums);
        break;
      case Mode::ParAtlas:
        stats = sweepParallelAtlas(level, order);
        break;
      case Mode::ParMapSerialCommit:
        stats = sweepParallelMapSerialCommit(level, order, &commitSeconds);
        break;
      case Mode::ParMapTwoLock:
        stats = sweepParallelMapTwoLock(level, order);
        break;
      }
      levelSeconds += stats.seconds;
      committed += stats.committedDelta;
      levelMoves += stats.moved;
      levelFastAccepts += stats.fastAccepts;
      if (stats.moved == 0) {
        // Neighbour-of-mover dirty marking alone is not sufficient for
        // convergence: a move changes the aggregates of both endpoint
        // modules, which stales the deltas of non-adjacent nodes bordering
        // them. Re-dirty everything and confirm with a full sweep; only a
        // clean full sweep is a single-move local optimum.
        if (verified)
          break;
        for (unsigned int u = 0; u < current->n; ++u)
          level.dirty[u].store(1, std::memory_order_relaxed);
        verified = true;
      } else {
        verified = false;
      }
    }
    const double afterL = level.currentCodelength(obj, 0.0);

    if (topLevel) {
      result.level0Seconds = levelSeconds;
      result.level0CommitSeconds = commitSeconds;
      result.level0Moves = levelMoves;
      result.level0FastAccepts = levelFastAccepts;
      result.level0Sweeps = sweeps + 1;
      result.consistency = std::abs(afterL - (before + committed));
      topLevel = false;
    }

    // Aggregate; stop when (almost) nothing merged
    std::vector<unsigned int> leafToCoarse(current->n);
    Graph coarse = aggregate(level, leafToCoarse);
    if (verboseLevels) {
      std::fprintf(stderr, "    level %u: %u -> %u nodes, %u sweeps, %.3fs, L %.6f -> %.6f\n",
                   levelIndex, current->n, coarse.n, sweeps + 1, levelSeconds, before, afterL);
    }
    const bool progress = coarse.n < current->n - std::max(1u, current->n / 1000);
    for (unsigned int u = 0; u < leafGraph.n; ++u)
      leafAssignment[u] = leafToCoarse[leafAssignment[u]];
    if (!progress || coarse.n <= 1)
      break;
    owned = std::move(coarse);
    current = &owned;
    ++levelIndex;
  }

  result.totalSeconds = seconds(tRun, Clock::now());

  // Final two-level codelengths from the leaf assignment, computed exactly.
  unsigned int numModules = 0;
  for (unsigned int u = 0; u < leafGraph.n; ++u)
    numModules = std::max(numModules, leafAssignment[u] + 1);
  std::vector<double> P(numModules, 0.0);
  std::vector<double> Q(numModules, 0.0);
  for (unsigned int u = 0; u < leafGraph.n; ++u)
    P[leafAssignment[u]] += leafGraph.p[u];
  for (unsigned int u = 0; u < leafGraph.n; ++u) {
    for (unsigned int k = leafGraph.offset[u]; k < leafGraph.offset[u + 1]; ++k) {
      if (leafAssignment[u] != leafAssignment[leafGraph.target[k]])
        Q[leafAssignment[u]] += leafGraph.flow[k];
    }
  }
  unsigned int nonEmpty = 0;
  for (unsigned int m = 0; m < numModules; ++m)
    if (P[m] > 0.0)
      ++nonEmpty;
  result.modules = nonEmpty;
  result.finalMap = codelength(Objective::MapEq, P, Q, leafEntropy);
  result.finalAtlas = codelength(Objective::Atlas, P, Q, leafEntropy);
  return result;
}

} // namespace

int main(int argc, char** argv)
{
  unsigned int numBlocks = 200;
  unsigned int blockSize = 1000;
  double kIn = 8.0;
  double kOut = 2.0;
  unsigned int seed = 1;
  unsigned int maxSweeps = 200;
  unsigned int warmupSweeps = 2;
  unsigned int reps = 5;
  bool sweepScalingOnly = false;
  std::string threadList = "1,2,4";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() { return std::string(argv[++i]); };
    if (arg == "--blocks")
      numBlocks = static_cast<unsigned int>(std::stoul(next()));
    else if (arg == "--block-size")
      blockSize = static_cast<unsigned int>(std::stoul(next()));
    else if (arg == "--kin")
      kIn = std::stod(next());
    else if (arg == "--kout")
      kOut = std::stod(next());
    else if (arg == "--seed")
      seed = static_cast<unsigned int>(std::stoul(next()));
    else if (arg == "--threads")
      threadList = next();
    else if (arg == "--verbose")
      verboseLevels = true;
    else if (arg == "--sweep-scaling")
      sweepScalingOnly = true;
    else if (arg == "--warmup-sweeps")
      warmupSweeps = static_cast<unsigned int>(std::stoul(next()));
    else if (arg == "--reps")
      reps = static_cast<unsigned int>(std::stoul(next()));
    else {
      std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
      return 1;
    }
  }

  std::vector<int> threads;
  {
    size_t pos = 0;
    while (pos < threadList.size()) {
      size_t comma = threadList.find(',', pos);
      if (comma == std::string::npos)
        comma = threadList.size();
      threads.push_back(std::stoi(threadList.substr(pos, comma - pos)));
      pos = comma + 1;
    }
  }

  Graph g = plantedPartition(numBlocks, blockSize, kIn, kOut, seed);
  std::printf("planted partition: %u nodes, %zu edges, %u blocks of %u (k_in=%.1f, k_out=%.1f)\n",
              g.n, g.target.size() / 2, numBlocks, blockSize, kIn, kOut);
  std::printf("one-level codelength: %.6f bits\n\n", g.nodeEntropy());
  std::printf("%-27s %8s | %10s %10s %8s %9s | %12s %12s %8s | %10s\n",
              "mode", "threads", "L0 time", "L0 commit", "L0 fast", "L0 sweeps", "final L_map", "final L_atlas", "modules", "|L-sumdelta|");

  if (sweepScalingOnly) {
    sweepScaling(g, threads, warmupSweeps, reps, seed + 100);
    return 0;
  }

  for (Mode mode : { Mode::SeqMap, Mode::SeqAtlas }) {
#ifdef _OPENMP
    omp_set_num_threads(1);
#endif
    RunResult r = run(g, mode, seed + 100, maxSweeps);
    std::printf("%-27s %8d | %9.3fs %9.3fs %8s %9u | %12.6f %12.6f %8u | %10.2e\n",
                modeName(mode), 1, r.level0Seconds, r.level0CommitSeconds, "-", r.level0Sweeps,
                r.finalMap, r.finalAtlas, r.modules, r.consistency);
  }
  for (Mode mode : { Mode::ParMapSerialCommit, Mode::ParAtlas, Mode::ParMapTwoLock }) {
    for (int t : threads) {
#ifdef _OPENMP
      omp_set_num_threads(t);
#endif
      RunResult r = run(g, mode, seed + 100, maxSweeps);
      char fastCol[16];
      if (r.level0Moves > 0 && r.level0FastAccepts > 0)
        std::snprintf(fastCol, sizeof fastCol, "%.0f%%", 100.0 * r.level0FastAccepts / r.level0Moves);
      else
        std::snprintf(fastCol, sizeof fastCol, "%s", "-");
      std::printf("%-27s %8d | %9.3fs %9.3fs %8s %9u | %12.6f %12.6f %8u | %10.2e\n",
                  modeName(mode), t, r.level0Seconds, r.level0CommitSeconds, fastCol, r.level0Sweeps,
                  r.finalMap, r.finalAtlas, r.modules, r.consistency);
    }
  }
  return 0;
}
