/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "core/InfomapCore.h"
#include "io/Config.h"
#include <iostream>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace infomap {

int run(const std::string& flags)
{
  try {
    Config conf(flags, true);

    InfomapCore infomap(conf);

    infomap.run();
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  } catch (char const* e) {
    std::cerr << "Str error: " << e << std::endl;
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
