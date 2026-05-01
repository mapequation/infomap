/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Infomap.h"
#include "io/ProgramInterface.h"
#ifndef AS_LIB
#include <iostream>
#endif
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {

int run(const std::string& flags)
{
  try {
    InfomapWrapper(Config(flags, true)).run();
  } catch (const CleanExit&) {
    // Help / version / json-parameters CLI flags requested a clean exit.
    return 0;
  } catch (std::exception& e) {
#ifndef AS_LIB
    std::cerr << "Error: " << e.what() << '\n';
#else
    (void)e;
#endif
    return 1;
  } catch (char const* e) {
#ifndef AS_LIB
    std::cerr << "Str error: " << e << '\n';
#else
    (void)e;
#endif
    return 1;
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
