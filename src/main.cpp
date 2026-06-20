/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Infomap.h"
#include "io/InfomapError.h"
#include "io/ProgramInterface.h"
#include "io/RunMetadata.h"
#include <iostream>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef AS_LIB
#include <csignal>
#endif

namespace infomap {

// Poll that run() installs on its InfomapWrapper, so the single-call entry (the
// pip `infomap` console script, via this AS_LIB free function) is interruptible.
// The Python binding registers a PyErr_CheckSignals poll here at module init;
// the native CLI uses its own below. Null = no polling (e.g. the R extension).
InterruptCallback g_runInterruptCallback = nullptr;

#ifndef AS_LIB
namespace {
  // Native CLI Ctrl-C (#412). The handler only assigns a volatile sig_atomic_t —
  // the one async-signal-safe operation under the C++14 core build (not a
  // std::atomic, which is signal-safe only from C++17). The owner-thread poll
  // reads it and flips the run's lock-free atomic cancel flag that workers see.
  volatile std::sig_atomic_t g_cliInterruptRequested = 0;
  extern "C" void cliHandleInterrupt(int) { g_cliInterruptRequested = 1; }
  bool cliInterruptPoll(void*) { return g_cliInterruptRequested != 0; }
} // namespace
#endif

int run(const std::string& flags)
{
  try {
    Config config;
    try {
      config = Config(flags, true);
    } catch (const InfomapError&) {
      // Preserve the typed exit code (e.g. output errors during validation).
      throw;
    } catch (const std::exception& e) {
      // Argument parsing failures surface as std::runtime_error.
      throw InfomapError(ExitCode::InvalidArguments, e.what());
    }
    // CleanExit (help/version/json-parameters) is not a std::exception, so it
    // bypasses the remapping above and is handled by the outer catch.

    if (config.printConfigFingerprint) {
      std::cout << configFingerprint(config) << '\n';
      return 0;
    }
    InfomapWrapper im(config);
#ifndef AS_LIB
    // Clear the flag first: a Ctrl-C from an earlier run() in this process must
    // not abort the next one at its first checkpoint.
    g_cliInterruptRequested = 0;
    std::signal(SIGINT, cliHandleInterrupt);
    im.setInterruptHandler(&cliInterruptPoll);
#else
    if (g_runInterruptCallback != nullptr)
      im.setInterruptHandler(g_runInterruptCallback);
#endif
    im.run();
  } catch (const CleanExit&) {
    // Help / version / json-parameters CLI flags requested a clean exit.
    return 0;
  } catch (const InterruptionError&) {
#ifdef AS_LIB
    throw; // let the binding surface it (Python: the pending KeyboardInterrupt)
#else
    std::cerr << "\nInterrupted.\n";
    return exitCodeValue(ExitCode::Interrupted);
#endif
  } catch (const InfomapError& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return exitCodeValue(e.code());
  } catch (const std::logic_error& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return exitCodeValue(ExitCode::InternalError);
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return exitCodeValue(ExitCode::InternalError);
  } catch (char const* e) {
    std::cerr << "Error: " << e << '\n';
    return exitCodeValue(ExitCode::InternalError);
  }

  return 0;
}

} // namespace infomap

#ifndef AS_LIB
int main(int argc, char* argv[])
{
  std::ostringstream args("");

  for (int i = 1; i < argc; ++i) {
    args << argv[i] << (i + 1 == argc ? "" : " ");
  }

  return infomap::run(args.str());
}
#endif
