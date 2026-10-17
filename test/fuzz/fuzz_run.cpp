// libFuzzer harness that runs Infomap end to end, with the flow invariants on.
//
// The first kHeaderSize bytes choose the options, the rest is the network,
// read through the same intake as fuzz_intake (Pajek-style or JSON). The
// header maps onto a bounded set of options that change the flow or the map
// equation: the flow models, recorded teleportation, Markov time, two-level,
// preferred number of modules, entropy correction, regularization, multilayer
// relaxation, bipartite teleportation and meta data. State, multilayer and
// bipartite input come from the body's own sections. Each run is one trial on
// a small network with a capped core loop, so an input stays fast.
//
// The fuzz build defines INFOMAP_CHECK_INVARIANTS, so the run checks the flow
// invariants after each phase (InfomapBase::checkInvariants). Contract:
// - malformed input or a rejected option combination throws an InfomapError
//   or a runtime_error, and is swallowed;
// - a broken invariant, an error that says it is a bug, or any
//   other exception out of the run aborts, so libFuzzer records it as a
//   finding next to crashes, sanitizer reports and hangs.
// A known open bug is recognised and swallowed rather than reported again: see
// isKnownFinding().

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "Infomap.h"
#include "io/InfomapError.h"

namespace {

constexpr size_t kHeaderSize = 8;
// Larger inputs only slow the fuzzer down; the intake fuzzer covers them.
constexpr unsigned int kMaxNodes = 64;
constexpr unsigned int kMaxLinks = 512;

bool contains(const char* haystack, const char* needle) { return std::strstr(haystack, needle) != nullptr; }

// The flow invariants, and the negative-flow guards on the finished tree, say in
// their message that they are a bug ("This is a bug in Infomap", "this is a bug
// in how module flow is accumulated", ...).
bool isBug(const char* message)
{
  return contains(message, "Flow invariant violated") || contains(message, "is a bug in");
}

// Picks one of the values by a few bits of the header.
template <size_t N>
const char* pick(const std::array<const char*, N>& values, uint8_t byte)
{
  return values[byte % N];
}

std::string optionsFromHeader(const uint8_t* h)
{
  std::string flags = "--silent --no-file-output -N 1";

  // Byte 0: flow model and teleportation.
  static const std::array<const char*, 6> flowModels = { "undirected", "directed", "undirdir", "outdirdir", "rawdir", "precomputed" };
  flags += " --flow-model ";
  flags += pick(flowModels, h[0] & 0x7);
  if ((h[0] & 0x08) != 0)
    flags += " --recorded-teleportation";
  if ((h[0] & 0x10) != 0)
    flags += " --to-nodes";
  if ((h[0] & 0x20) != 0)
    flags += " --two-level";
  if ((h[0] & 0x40) != 0)
    flags += " --regularized";
  if ((h[0] & 0x80) != 0)
    flags += " --entropy-corrected";

  // Byte 1: Markov time and teleportation probability.
  static const std::array<const char*, 6> markovTimes = { "", " --markov-time 0.5", " --markov-time 2", " --markov-time 0.01", " --markov-time 10", " --variable-markov-time" };
  flags += pick(markovTimes, h[1] & 0x7);
  static const std::array<const char*, 5> teleportation = { "", " -p 0.05", " -p 0.5", " -p 0", " -p 1" };
  flags += pick(teleportation, (h[1] >> 3) & 0x7);
  if ((h[1] & 0x40) != 0)
    flags += " --use-node-weights-as-flow";
  if ((h[1] & 0x80) != 0)
    flags += " --include-self-links";

  // Byte 2: the number and depth of modules.
  const unsigned int preferredModules = h[2] & 0x7;
  if (preferredModules != 0)
    flags += " --preferred-number-of-modules " + std::to_string(preferredModules);
  static const std::array<const char*, 4> preferredLevels = { "", " --preferred-number-of-levels 2", " --preferred-number-of-levels 3", " --fast-hierarchical-solution 1" };
  flags += pick(preferredLevels, (h[2] >> 3) & 0x3);
  if ((h[2] & 0x20) != 0)
    flags += " --prefer-modular-solution";
  static const std::array<const char*, 4> strength = { "", " --regularization-strength 0.1", " --entropy-correction-strength 2", " --regularization-strength 10" };
  flags += pick(strength, (h[2] >> 6) & 0x3);

  // Byte 3: multilayer relaxation.
  static const std::array<const char*, 5> relaxRates = { "", " --multilayer-relax-rate 0", " --multilayer-relax-rate 0.15", " --multilayer-relax-rate 1", " --multilayer-relax-by-jsd" };
  flags += pick(relaxRates, h[3] & 0x7);
  static const std::array<const char*, 4> relaxLimits = { "", " --multilayer-relax-limit 1", " --multilayer-relax-limit-up 0", " --multilayer-relax-limit-down 1" };
  flags += pick(relaxLimits, (h[3] >> 3) & 0x3);
  if ((h[3] & 0x20) != 0)
    flags += " --multilayer-relax-to-self";
  if ((h[3] & 0x40) != 0)
    flags += " --matchable-multilayer-ids 4";

  // Byte 4: bipartite handling and link filtering.
  if ((h[4] & 0x01) != 0)
    flags += " --bipartite-teleportation";
  if ((h[4] & 0x02) != 0)
    flags += " --skip-adjust-bipartite-flow";
  if ((h[4] & 0x04) != 0)
    flags += " --no-self-links";
  static const std::array<const char*, 3> weightThresholds = { "", " --weight-threshold 0.5", " --weight-threshold 2" };
  flags += pick(weightThresholds, (h[4] >> 3) & 0x3);

  // Byte 5: meta data (the categories are added in applyMetaData()).
  if ((h[5] & 0x7) != 0) {
    static const std::array<const char*, 4> metaRates = { "", " --meta-data-rate 0", " --meta-data-rate 0.5", " --meta-data-rate 4" };
    flags += pick(metaRates, (h[5] >> 3) & 0x3);
    if ((h[5] & 0x20) != 0)
      flags += " --meta-data-unweighted";
  }

  // Byte 6: seed.
  flags += " --seed " + std::to_string(1 + h[6]);

  // Byte 7: search effort, kept small.
  flags += " --core-loop-limit " + std::to_string(1 + (h[7] & 0x7));
  flags += " --tune-iteration-limit " + std::to_string(1 + ((h[7] >> 3) & 0x3));
  if ((h[7] & 0x20) != 0)
    flags += " --assign-to-neighbouring-module";
  if ((h[7] & 0x40) != 0)
    flags += " --core-level-limit 1";
  return flags;
}

using MetaData = std::vector<std::pair<unsigned int, int>>;

// Byte 5, bits 0-2: number of meta-data categories (0 = none), one per node,
// cycled over the state ids with the stride in bits 6-7.
MetaData applyMetaData(infomap::InfomapWrapper& im, uint8_t byte)
{
  MetaData metaData;
  const unsigned int numCategories = byte & 0x7;
  if (numCategories == 0)
    return metaData;
  for (const auto& node : im.network().nodes())
    metaData.emplace_back(node.first, static_cast<int>((metaData.size() * (1 + (byte >> 6))) % numCategories));
  for (const auto& record : metaData)
    im.addMetaData(record.first, record.second);
  return metaData;
}

// Open bugs the target would otherwise report on every run. Each is pinned as a
// should_fail case in test/cpp/test_map_equation_invariants.cpp ("Fuzz finding:")
// or by its issue; drop the entry when the bug is fixed so the fuzzer guards the fix.
bool isKnownFinding(const infomap::InfomapWrapper& im)
{
  // #1175: bipartite input with recorded teleportation (also implied by
  // --regularized) leaves the teleport weights summing to less than 1.
  if (im.network().isBipartite() && (im.recordedTeleportation || im.regularized))
    return true;
  // --regularized with Markov time above 1, global or variable: negative module
  // enter flow.
  if (im.regularized && (im.markovTime > 1.0 || im.variableMarkovTime))
    return true;
  // --recorded-teleportation under the undirdir, outdirdir and rawdir models: leaf
  // exit and enter flow do not balance.
  const auto model = im.flowModel;
  if (im.recordedTeleportation && !im.regularized && (model == infomap::FlowModel::undirdir || model == infomap::FlowModel::outdirdir || model == infomap::FlowModel::rawdir))
    return true;
  // --regularized with --use-node-weights-as-flow: negative enter flow where a node's
  // weight gives it less flow than its links carry.
  return im.regularized && im.useNodeWeightsAsFlow;
}

// Prints what the CLI needs to replay the finding: the options, and the meta
// data as a --meta-data file, then aborts.
[[noreturn]] void reportBug(const std::string& flags, const MetaData& metaData, const char* message)
{
  std::fprintf(stderr, "fuzz_run: options: %s\n", flags.c_str());
  if (!metaData.empty()) {
    std::fprintf(stderr, "fuzz_run: meta data (node category):\n");
    for (const auto& record : metaData)
      std::fprintf(stderr, "%u %d\n", record.first, record.second);
  }
  std::fprintf(stderr, "fuzz_run: %s\n", message);
  std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  if (size < kHeaderSize)
    return 0;
  const std::string flags = optionsFromHeader(data);

  char path[] = "/tmp/infomap_fuzz_run_XXXXXX";
  const int fd = mkstemp(path);
  if (fd < 0)
    return 0;
  ::close(fd);
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data + kHeaderSize), static_cast<std::streamsize>(size - kHeaderSize));
  }

  std::unique_ptr<infomap::InfomapWrapper> im;
  try {
    im = std::make_unique<infomap::InfomapWrapper>(flags);
    im->readInputData(path);
  } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
    // Rejected options or malformed input: the intake fuzzer's ground.
    im.reset();
  }
  std::remove(path);
  if (!im)
    return 0;

  const auto& network = im->network();
  if (network.numNodes() == 0 || network.numNodes() > kMaxNodes || network.numLinks() > kMaxLinks)
    return 0;

  MetaData metaData;
  try {
    metaData = applyMetaData(*im, data[5]);
    im->run();
  } catch (const infomap::InfomapError& e) {
    if (isBug(e.what()) && !isKnownFinding(*im))
      reportBug(flags, metaData, e.what());
    // Otherwise an expected rejection of the input or the options.
  } catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch)
    // Validation that throws a plain runtime_error, such as a missing node
    // flow under --flow-model precomputed: also a rejection.
  } catch (const std::exception& e) {
    // A logic_error (out_of_range, invalid_argument, ...) or bad_alloc on a
    // network this small is a bug.
    reportBug(flags, metaData, e.what());
  }
  return 0;
}
