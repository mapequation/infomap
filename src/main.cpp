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

// Process-global interrupt poll for the single-call run(flags) CLI entry.
// run() installs it on the InfomapWrapper it creates, so the pip-installed
// `infomap` console script (which routes through this AS_LIB free function) is
// interruptible too. The Python binding registers a PyErr_CheckSignals poll in
// its module init; the native CLI sets its own SIGINT-flag poll below. Default
// null = no polling (unchanged behavior, e.g. the R extension never sets it).
InterruptCallback g_runInterruptCallback = nullptr;

#ifndef AS_LIB
namespace {
  // CLI cooperative Ctrl-C (issue #412). The handler only assigns a
  // volatile sig_atomic_t — the single operation guaranteed async-signal-safe in
  // every C++ standard (the core/CLI builds as C++14). The run polls it on the
  // main thread through the interrupt hook and cancels cleanly at the next
  // checkpoint, instead of the default SIGINT hard-killing the process mid-run
  // (which could leave a half-written output file). Cross-thread visibility to
  // the OpenMP workers is handled downstream: the main-thread poll flips the
  // run's lock-free atomic cancel flag, which the workers observe. Library builds
  // (-DAS_LIB, used by the Python/R bindings) never install this — they own their
  // own signal handling — so the shared infomap::run() entry is untouched there.
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
    // Native CLI: install a SIGINT-flag poll directly. Clear the process-global
    // flag first so a Ctrl-C from an earlier run() in the same process can't
    // make this run abort at its first checkpoint (run() can be called more than
    // once when embedded; the core resets its own atomic but not this flag).
    g_cliInterruptRequested = 0;
    std::signal(SIGINT, cliHandleInterrupt);
    im.setInterruptHandler(&cliInterruptPoll);
#else
    // Library builds (Python console script): poll whatever the binding registered.
    if (g_runInterruptCallback != nullptr)
      im.setInterruptHandler(g_runInterruptCallback);
#endif
    im.run();
  } catch (const CleanExit&) {
    // Help / version / json-parameters CLI flags requested a clean exit.
    return 0;
  } catch (const InterruptionError&) {
    // Ctrl-C / cooperative cancellation.
#ifdef AS_LIB
    // Let the binding surface it (Python: the pending KeyboardInterrupt set by
    // PyErr_CheckSignals propagates; see the run() %exception in Infomap.i).
    throw;
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
