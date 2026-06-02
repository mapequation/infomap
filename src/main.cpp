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

namespace infomap {

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
    InfomapWrapper(config).run();
  } catch (const CleanExit&) {
    // Help / version / json-parameters CLI flags requested a clean exit.
    return 0;
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
    std::cerr << "Str error: " << e << '\n';
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
