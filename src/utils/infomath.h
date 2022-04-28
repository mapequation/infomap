/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Eriksson, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_AGPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef INFOMATH_H_
#define INFOMATH_H_

#include <cmath>

namespace infomap {
namespace infomath {

  using std::log2;

  inline double plogp(double p)
  {
    return p > 0.0 ? p * log2(p) : 0.0;
  }

} // namespace infomath
} // namespace infomap

#endif // INFOMATH_H_
