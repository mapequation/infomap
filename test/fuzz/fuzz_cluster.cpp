// libFuzzer harness for cluster data input (--cluster-data), with the flow
// invariants on.
//
// The first kHeaderSize bytes choose one of a few small fixed networks, the
// format of the cluster file and the options; the rest is the cluster file, a
// .clu, .tree or .ftree. The options are the ones that change how cluster data
// is used: hard or soft partitions, --no-infomap scoring, two-level or
// multilevel, directed flow, and a bounded set of objectives.
//
// The fuzz build defines INFOMAP_CHECK_INVARIANTS, so the run checks the flow
// invariants after each phase. Contract:
// - malformed cluster data throws an InfomapError or a runtime_error, and is
//   swallowed;
// - a broken invariant, an error that says it is a bug, or any other exception
//   out of the run aborts, so libFuzzer records it as a finding next to
//   crashes, sanitizer reports and hangs;
// - so does a run whose result disagrees with its input or with itself:
//   * the reported codelength must equal calcCodelengthOnTree on the finished
//     tree;
//   * scoring the finished tree again, written as a .tree file and read with
//     --no-infomap by a fresh instance, must give the same codelength;
//   * --no-infomap must keep the partition it was given, and a hard partition
//     must keep each of its modules together.
// A known open bug is recognised and swallowed rather than reported again: see
// isKnownFinding().
//
// calcCodelengthOnTree and normalizeTreePaths are private on InfomapBase; this
// target is compiled with -fno-access-control (see CMakeLists.txt) to reach
// them, as the invariant tests are.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <ios>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "Infomap.h"
#include "io/ClusterMap.h"
#include "io/InfomapError.h"

namespace {

constexpr size_t kHeaderSize = 3;

struct FixedNetwork {
  const char* name;
  const char* text;
};

// Small networks that cover the ways cluster data is matched to nodes:
// first-order ids (contiguous or not, with dangling and isolated nodes),
// state ids with repeated physical ids, multilayer (layer, node) keys, and
// bipartite input. Kept small so a run stays fast.
const std::array<FixedNetwork, 8> kNetworks = { {
    { "twotriangles", "*Vertices\n1 \"A\"\n2 \"B\"\n3 \"C\"\n4 \"D\"\n5 \"E\"\n6 \"F\"\n*Edges\n1 2\n1 3\n2 3\n3 4\n4 5\n4 6\n5 6\n" },
    { "ninetriangles", "1 2\n1 3\n2 3\n4 5\n4 6\n5 6\n7 8\n7 9\n8 9\n2 4\n3 7\n6 8\n10 11\n10 12\n11 12\n13 14\n13 15\n14 15\n"
                       "16 17\n16 18\n17 18\n11 13\n12 16\n15 17\n19 20\n19 21\n20 21\n22 23\n22 24\n23 24\n25 26\n25 27\n26 27\n"
                       "20 22\n21 25\n24 26\n5 10 0.8\n9 19 0.8\n18 23 0.8\n" },
    { "states", "*Vertices 5\n1 \"i\"\n2 \"j\"\n3 \"k\"\n4 \"l\"\n5 \"m\"\n*States\n1 1 \"a_i\"\n2 2 \"b_j\"\n3 3 \"c_k\"\n4 1 \"d_i\"\n5 4 \"e_l\"\n6 5 \"f_m\"\n"
                "*Links\n1 2 0.8\n1 3 0.8\n1 5 0.2\n1 6 0.2\n2 1 1\n2 3 1\n3 1 1\n3 2 1\n4 5 0.8\n4 6 0.8\n4 2 0.2\n4 3 0.2\n5 4 1\n5 6 1\n6 4 1\n6 5 1\n" },
    { "multilayer", "*Vertices 5\n1 \"i\"\n2 \"j\"\n3 \"k\"\n4 \"l\"\n5 \"m\"\n*Multilayer\n1 1 1 4 0.8\n1 4 1 1 1\n1 1 1 5 0.8\n1 5 1 1 1\n1 4 1 5 1\n1 5 1 4 1\n"
                    "2 1 2 2 0.8\n2 2 2 1 1\n2 1 2 3 0.8\n2 3 2 1 1\n2 2 2 3 1\n2 3 2 2 1\n1 1 2 2 0.2\n1 1 2 3 0.2\n2 1 1 4 0.2\n2 1 1 5 0.2\n" },
    // Non-contiguous ids, a self-link, a dangling node (9) and an isolated node (13).
    { "sparse-ids", "*Vertices\n1 \"a\"\n2 \"b\"\n3 \"c\"\n5 \"e\"\n8 \"h\"\n9 \"i\"\n13 \"m\"\n*Arcs\n1 2 1\n2 3 1\n3 1 1\n3 5 0.5\n5 8 1\n8 5 1\n8 8 0.5\n8 9 0.2\n" },
    { "unbalanced", "*Edges\n1 2 10\n1 3 10\n2 3 10\n4 5 10\n4 6 10\n5 6 10\n7 8 10\n7 9 10\n8 9 10\n3 4 0.3\n6 7 0.3\n9 1 0.3\n"
                    "10 11 1\n10 12 1\n10 13 1\n11 12 1\n12 16 1\n13 14 1\n13 15 1\n14 15 1\n15 17 1\n16 17 1\n16 18 1\n17 18 1\n"
                    "19 20 1\n19 21 1\n19 22 1\n20 21 1\n21 25 1\n22 23 1\n22 24 1\n23 24 1\n24 26 1\n25 26 1\n25 27 1\n26 27 1\n"
                    "28 29 1\n28 30 1\n28 31 1\n29 30 1\n30 34 1\n31 32 1\n31 33 1\n32 33 1\n33 35 1\n34 35 1\n34 36 1\n35 36 1\n"
                    "18 19 0.5\n27 28 0.5\n36 10 0.5\n7 10 0.05\n" },
    { "bipartite", "*Vertices 5\n1 \"n1\"\n2 \"n2\"\n3 \"n3\"\n4 \"f1\"\n5 \"f2\"\n*Bipartite 4\n1 4 1\n2 4 1\n2 5 0.25\n3 5 1\n" },
    { "multilayer-intra-inter", "*Vertices 5\n1 \"i\"\n2 \"j\"\n3 \"k\"\n4 \"l\"\n5 \"m\"\n*Intra\n1 1 4 0.8\n1 4 1 1\n1 1 5 0.8\n1 5 1 1\n1 4 5 1\n1 5 4 1\n"
                                "2 1 2 0.8\n2 2 1 1\n2 1 3 0.8\n2 3 1 1\n2 2 3 1\n2 3 2 1\n*Inter\n1 1 2 0.4\n2 1 1 0.4\n" },
} };

struct Options {
  unsigned int network = 0;
  const char* extension = "clu";
  bool hard = false;
  bool noInfomap = false;
  bool metaData = false;
  std::string flags; // without the network and the cluster data file
};

// Picks one of the values by a few bits of the header.
template <size_t N>
const char* pick(const std::array<const char*, N>& values, uint8_t byte)
{
  return values[byte % N];
}

Options optionsFromHeader(const uint8_t* h)
{
  Options options;
  std::string& flags = options.flags;
  flags = "--silent --no-file-output";

  // Byte 0: network, cluster file format, how the cluster data is used.
  options.network = h[0] & 0x7;
  static const std::array<const char*, 4> extensions = { "clu", "tree", "ftree", "clu" };
  options.extension = pick(extensions, (h[0] >> 3) & 0x3);
  options.hard = (h[0] & 0x20) != 0;
  options.noInfomap = (h[0] & 0x40) != 0;
  if (options.noInfomap)
    flags += " --no-infomap";
  if ((h[0] & 0x80) != 0)
    flags += " --two-level";

  // Byte 1: flow and objective, search effort.
  if ((h[1] & 0x01) != 0)
    flags += " -d";
  static const std::array<const char*, 8> objectives = { "", " --entropy-corrected", " --regularized", " --preferred-number-of-modules 2", " --markov-time 0.5", "", " --recorded-teleportation", " --markov-time 2" };
  const unsigned int objective = (h[1] >> 1) & 0x7;
  flags += objectives[objective];
  options.metaData = objective == 5; // categories added in applyMetaData()
  if ((h[1] & 0x10) != 0)
    flags += " --assign-to-neighbouring-module";
  flags += (h[1] & 0x20) != 0 ? " -N 2" : " -N 1";
  flags += (h[1] & 0x40) != 0 ? " --core-loop-limit 1" : " --core-loop-limit 4";
  if ((h[1] & 0x80) != 0)
    flags += " --core-level-limit 1";

  // Byte 2: seed.
  flags += " --seed " + std::to_string(1 + h[2]);
  return options;
}

using MetaData = std::vector<std::pair<unsigned int, int>>;

// Two meta-data categories, alternating over the state ids.
MetaData applyMetaData(infomap::InfomapWrapper& im, bool enabled)
{
  MetaData metaData;
  if (!enabled)
    return metaData;
  for (const auto& node : im.network().nodes())
    metaData.emplace_back(node.first, static_cast<int>(metaData.size() % 2));
  for (const auto& record : metaData)
    im.addMetaData(record.first, record.second);
  return metaData;
}

bool contains(const char* haystack, const char* needle) { return std::strstr(haystack, needle) != nullptr; }

// The flow invariants, and the negative-flow guards on the finished tree, say in
// their message that they are a bug.
bool isBug(const char* message)
{
  return contains(message, "Flow invariant violated") || contains(message, "is a bug in");
}

// A failed check of the harness's own, raised as a bug.
struct Finding : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Open bugs the target would otherwise report on every run. Each needs its options
// and its kind of failure, so an unrelated failure under the same options is still
// reported. Drop the entry when the bug is fixed so the fuzzer guards the fix.
bool isKnownFinding(const infomap::InfomapWrapper& im, const char* message)
{
  const bool negative = contains(message, "Negative");
  const bool unbalanced = contains(message, "do not balance");
  const bool misreported = contains(message, "reported codelength");
  // #1175: bipartite input with recorded teleportation (also implied by
  // --regularized) leaves the teleport weights summing to less than 1.
  if (im.network().isBipartite() && (im.recordedTeleportation || im.regularized) && (negative || unbalanced))
    return true;
  // Hard partitions are dropped when the clu file misses a node: initPartition passes
  // `hard` on only when every node has a module.
  if (im.clusterDataIsHard && contains(message, "hard partition was split") && contains(message, "misses"))
    return true;
  // The tree is not scored again after the hard partition is restored: a module that held
  // a single hard module is left without that module's codebook, and a hard module
  // directly under the root (all of them under --no-infomap) is replaced by its nodes there.
  if (im.clusterDataIsHard && misreported)
    return true;
  // On a state network, a search from a tree that it reshapes reports the codelength of
  // the memory map equation's stale per-module physical-node bookkeeping, which
  // calcCodelengthOnTree on the same instance repeats; a fresh instance scores less.
  if (im.haveMemory() && !im.noInfomap && misreported && contains(message, "read back"))
    return true;
  // Under undirected flow with --recorded-teleportation, a tree of three or more levels
  // built by initTree (a .tree read as cluster data, or the deep seed of a search) gets
  // modules of modules whose enter and exit flow do not balance: the recorded-teleportation
  // terms in aggregateFlowValuesFromLeafToRoot are added on top of undirected link flow.
  if (im.recordedTeleportation && !im.regularized && im.isUndirectedClustering() && unbalanced)
    return true;
  // #1176: the recursive search leaves the entropy correction off the modules of
  // modules, and the |K - K_pref| term of --preferred-number-of-modules off the
  // total, so a searched tree is reported short of its own codelength (fixed by #1178).
  return misreported && !im.noInfomap && (im.entropyBiasCorrection || im.preferredNumberOfModules != 0);
}

std::string writeTemp(const char* prefix, const char* suffix, const char* data, size_t size)
{
  std::string path = std::string("/tmp/infomap_fuzz_cluster_") + prefix + "_XXXXXX" + suffix;
  std::vector<char> buffer(path.begin(), path.end());
  buffer.push_back('\0');
  const int fd = mkstemps(buffer.data(), static_cast<int>(std::strlen(suffix)));
  if (fd < 0)
    return { };
  ::close(fd);
  std::ofstream out(buffer.data(), std::ios::binary);
  out.write(data, static_cast<std::streamsize>(size));
  return buffer.data();
}

struct TempFile {
  std::string path;
  ~TempFile()
  {
    if (!path.empty())
      std::remove(path.c_str());
  }
};

using Path = std::vector<unsigned int>;

struct Leaf {
  const infomap::InfoNode* node;
  const infomap::InfoNode* parent;
  Path path; // 1-based child index at each depth
};

void collectLeaves(const infomap::InfoNode& node, Path& path, std::vector<Leaf>& leaves)
{
  unsigned int childIndex = 0;
  for (const infomap::InfoNode& child : node) {
    path.push_back(++childIndex);
    if (child.isLeaf())
      leaves.push_back({ &child, &node, path });
    else
      collectLeaves(child, path, leaves);
    path.pop_back();
  }
}

std::vector<Leaf> leavesOf(const infomap::InfoNode& root)
{
  std::vector<Leaf> leaves;
  Path path;
  collectLeaves(root, path, leaves);
  return leaves;
}

bool sameCodelength(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max({ 1.0, std::abs(a), std::abs(b) }); }

std::string formatCodelength(double value)
{
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.12g", value);
  return buffer;
}

// The finished tree as a state-level .tree file Infomap can read back.
std::string treeText(const std::vector<Leaf>& leaves, const infomap::Network& network)
{
  std::ostringstream out;
  out.precision(17);
  const bool multilayer = network.isMultilayerNetwork();
  const bool states = multilayer || network.haveMemoryInput();
  for (const auto& leaf : leaves) {
    for (size_t i = 0; i < leaf.path.size(); ++i)
      out << (i == 0 ? "" : ":") << leaf.path[i];
    out << ' ' << leaf.node->data.flow << " \"n\" " << leaf.node->stateId;
    if (states)
      out << ' ' << leaf.node->physicalId;
    if (multilayer)
      out << ' ' << leaf.node->layerId;
    out << '\n';
  }
  return out.str();
}

// Length of the common prefix of the module paths (the path without the leaf's own
// index) of two leaves: the depth of the deepest module they share.
size_t sharedDepth(const Path& a, const Path& b)
{
  const size_t n = std::min(a.size(), b.size()) - 1;
  size_t depth = 0;
  while (depth < n && a[depth] == b[depth])
    ++depth;
  return depth;
}

// --no-infomap keeps the partition it was given: two nodes share a module at each
// depth in the finished tree exactly when they do in the cluster data. Only the
// nodes the cluster data names once are compared; the rest are placed by rules of
// their own. Under --two-level only the top level is kept.
void checkKeepsPartition(infomap::InfomapWrapper& im, const Options& options, const std::string& clusterPath, const std::vector<Leaf>& leaves)
{
  infomap::ClusterMap clusterMap;
  const auto& network = im.network();
  if (network.isMultilayerNetwork())
    clusterMap.readClusterData(clusterPath, false, &network.layerNodeToStateId());
  else
    clusterMap.readClusterData(clusterPath);

  std::map<unsigned int, Path> given;
  std::map<unsigned int, unsigned int> rows;
  if (clusterMap.extension() == "clu") {
    for (const auto& entry : clusterMap.clusterIds())
      given[entry.first] = { entry.second, 1 };
  } else {
    unsigned int numNotInNetwork = 0;
    for (const auto& nodePath : im.normalizeTreePaths(clusterMap.treePaths(), numNotInNetwork)) {
      given[nodePath.first] = nodePath.second;
      ++rows[nodePath.first];
    }
  }

  std::vector<std::pair<const Path*, const Path*>> compared; // given, found
  for (const auto& leaf : leaves) {
    const auto it = given.find(leaf.node->stateId);
    if (it != given.end() && it->second.size() >= 2 && rows[leaf.node->stateId] <= 1)
      compared.emplace_back(&it->second, &leaf.path);
  }

  const size_t maxDepth = im.twoLevel ? 1 : static_cast<size_t>(-1);
  for (size_t i = 0; i < compared.size(); ++i) {
    for (size_t j = i + 1; j < compared.size(); ++j) {
      const auto givenDepth = std::min(maxDepth, sharedDepth(*compared[i].first, *compared[j].first));
      const auto foundDepth = std::min(maxDepth, sharedDepth(*compared[i].second, *compared[j].second));
      if (givenDepth != foundDepth)
        throw Finding("--no-infomap changed the partition: two nodes share " + std::to_string(givenDepth) + " module levels in the " + options.extension + " file but " + std::to_string(foundDepth) + " in the finished tree");
    }
  }
}

// A hard partition is never split: each of its modules ends up as the children of
// one module in the finished tree.
void checkKeepsHardModules(infomap::InfomapWrapper& im, const std::string& clusterPath, const std::vector<Leaf>& leaves)
{
  infomap::ClusterMap clusterMap;
  const auto& network = im.network();
  if (network.isMultilayerNetwork())
    clusterMap.readClusterData(clusterPath, false, &network.layerNodeToStateId());
  else
    clusterMap.readClusterData(clusterPath);
  const auto& clusterIds = clusterMap.clusterIds();

  std::map<unsigned int, const infomap::InfoNode*> parentOfModule;
  unsigned int numMissing = 0;
  bool split = false;
  unsigned int splitModule = 0;
  for (const auto& leaf : leaves) {
    const auto it = clusterIds.find(leaf.node->stateId);
    if (it == clusterIds.end()) {
      ++numMissing;
      continue;
    }
    const auto inserted = parentOfModule.insert({ it->second, leaf.parent });
    if (!split && !inserted.second && inserted.first->second != leaf.parent) {
      split = true;
      splitModule = it->second;
    }
  }
  if (split)
    throw Finding("a hard partition was split: module " + std::to_string(splitModule) + " of the clu file is spread over more than one module" + (numMissing > 0 ? " (the clu file misses " + std::to_string(numMissing) + " nodes)" : ""));
}

// Scores the finished tree in a fresh instance: written as a .tree file and read
// back with --no-infomap. Returns false when that run rejects the tree.
bool rescore(const Options& options, const std::string& networkPath, const std::vector<Leaf>& leaves, const infomap::Network& network, double& codelength, std::string& flags)
{
  const std::string text = treeText(leaves, network);
  TempFile tree { writeTemp("rescore", ".tree", text.data(), text.size()) };
  if (tree.path.empty())
    return false;
  // -N 1: the tree is scored as it is, the trials would only repeat that.
  flags = options.flags + " --no-infomap -N 1 -c " + tree.path;
  infomap::InfomapWrapper im(flags);
  im.readInputData(networkPath);
  applyMetaData(im, options.metaData);
  im.run();
  codelength = im.codelength();
  return true;
}

// A tree row whose path has no number in front is read as an empty path, which
// initTree indexes: a crash the harness cannot catch, so it skips such input.
bool hasEmptyTreePath(const infomap::Network& network, const std::string& clusterPath)
{
  infomap::ClusterMap clusterMap;
  try {
    if (network.isMultilayerNetwork())
      clusterMap.readClusterData(clusterPath, false, &network.layerNodeToStateId());
    else
      clusterMap.readClusterData(clusterPath);
  } catch (const std::exception&) {
    return false; // rejected, and the run rejects it the same way
  }
  return std::any_of(clusterMap.treePaths().begin(), clusterMap.treePaths().end(), [](const auto& treePath) { return treePath.path.empty(); });
}

// Prints what the CLI needs to replay the finding, then aborts.
[[noreturn]] void reportBug(const Options& options, const std::string& clusterText, const char* message)
{
  std::fprintf(stderr, "fuzz_cluster: network: %s\n", kNetworks[options.network].name);
  std::fprintf(stderr, "fuzz_cluster: options: %s%s%s\n", options.flags.c_str(), options.hard ? " (hard partition)" : "", options.metaData ? " (meta data: state ids alternate between categories 0 and 1)" : "");
  std::fprintf(stderr, "fuzz_cluster: cluster data (.%s):\n%s\n", options.extension, clusterText.c_str());
  std::fprintf(stderr, "fuzz_cluster: %s\n", message);
  std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  if (size < kHeaderSize)
    return 0;
  const Options options = optionsFromHeader(data);
  const std::string clusterText(reinterpret_cast<const char*>(data + kHeaderSize), size - kHeaderSize);
  const auto& network = kNetworks[options.network];

  TempFile networkFile { writeTemp("net", ".net", network.text, std::strlen(network.text)) };
  const std::string suffix = std::string(".") + options.extension;
  TempFile clusterFile { writeTemp("data", suffix.c_str(), clusterText.data(), clusterText.size()) };
  if (networkFile.path.empty() || clusterFile.path.empty())
    return 0;

  std::unique_ptr<infomap::InfomapWrapper> owner;
  try {
    owner = std::make_unique<infomap::InfomapWrapper>(options.flags + " -c " + clusterFile.path);
    owner->clusterDataIsHard = options.hard;
    owner->readInputData(networkFile.path);
  } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
    // A rejected option combination for this network: not what this target is for.
    return 0;
  }
  auto& im = *owner;
  if (hasEmptyTreePath(im.network(), clusterFile.path))
    return 0;

  try {
    applyMetaData(im, options.metaData);
    im.run();
  } catch (const infomap::InfomapError& e) {
    if (isBug(e.what()) && !isKnownFinding(im, e.what()))
      reportBug(options, clusterText, e.what());
    // Otherwise an expected rejection of the cluster data.
    return 0;
  } catch (const std::runtime_error&) {
    // The cluster data readers throw a plain runtime_error on malformed input.
    return 0;
  } catch (const std::exception& e) {
    // A logic_error (out_of_range, invalid_argument, ...) or bad_alloc on a
    // network this small is a bug.
    reportBug(options, clusterText, e.what());
  }

  try {
    const auto leaves = leavesOf(im.root());
    // A leaf directly under the root reads back as a module of its own (#1082), so
    // such a tree is not written back.
    const bool leafUnderRoot = std::any_of(leaves.begin(), leaves.end(), [](const Leaf& leaf) { return leaf.path.size() == 1; });
    const char* shape = leafUnderRoot ? " (a leaf sits directly under the root)" : "";
    const double reported = im.codelength();
    const double recomputed = im.calcCodelengthOnTree(im.root(), true);
    if (!sameCodelength(reported, recomputed))
      throw Finding("reported codelength " + formatCodelength(reported) + " but the finished tree scores " + formatCodelength(recomputed) + shape);

    if (options.hard && std::strcmp(options.extension, "clu") == 0)
      checkKeepsHardModules(im, clusterFile.path, leaves);
    if (options.noInfomap && !options.hard)
      checkKeepsPartition(im, options, clusterFile.path, leaves);

    if (!leafUnderRoot) {
      double rescored = 0.0;
      std::string flags;
      if (rescore(options, networkFile.path, leaves, im.network(), rescored, flags) && !sameCodelength(reported, rescored))
        throw Finding("reported codelength " + formatCodelength(reported) + " but the finished tree, read back with " + flags + ", scores " + formatCodelength(rescored));
    }
  } catch (const Finding& e) {
    if (!isKnownFinding(im, e.what()))
      reportBug(options, clusterText, e.what());
  } catch (const infomap::InfomapError& e) {
    if (!isKnownFinding(im, e.what()))
      reportBug(options, clusterText, (std::string("the finished tree could not be scored again: ") + e.what()).c_str());
  } catch (const std::exception& e) {
    reportBug(options, clusterText, (std::string("the finished tree could not be scored again: ") + e.what()).c_str());
  }
  return 0;
}
