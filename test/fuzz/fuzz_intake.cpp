// libFuzzer harness for the network intake.
//
// Feeds arbitrary bytes through the real intake entry point
// (buildNetworkFromInput), which sniffs a leading '{' to route between the JSON
// and the Pajek-style parsers -- so this single target exercises both parsers
// exactly as the CLI does. The intake reads from a path, so each input is
// written to a temporary file.
//
// Contract: malformed input is expected to throw (InfomapError /
// std::runtime_error) and is swallowed. Crashes, sanitizer reports, hangs, and
// exceptions of any other type are genuine findings.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <unistd.h>

#include "io/Network.h"
#include "io/NetworkBuilder.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  char path[] = "/tmp/infomap_fuzz_XXXXXX";
  const int fd = mkstemp(path);
  if (fd < 0)
    return 0;
  ::close(fd);

  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  }

  try {
    infomap::Network network("--silent --no-file-output");
    infomap::buildNetworkFromInput(network, path);
  } catch (const std::exception&) {  // NOLINT(bugprone-empty-catch)
    // Expected rejection path for malformed input; only crashes/sanitizer
    // reports/hangs are findings.
  }

  std::remove(path);
  return 0;
}
