/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Log.h"
#ifndef AS_LIB
#include <iostream>
#endif
#include <streambuf>

namespace infomap {

namespace {

class NullStreambuf : public std::streambuf {
protected:
  int_type overflow(int_type c) override { return traits_type::not_eof(c); }
};

} // namespace

std::ostream& Log::defaultStream()
{
#ifdef AS_LIB
  // Library builds (R, Python, JS) must not pull in std::cout to avoid
  // referencing libc++ globals that R CMD check --as-cran flags. Embedders
  // should call setOutputStream() to route logging into their host runtime
  // (e.g. Rprintf-backed streambuf for the R bindings).
  static NullStreambuf nullBuf;
  static std::ostream s(&nullBuf);
  return s;
#else
  static std::ostream& s = std::cout;
  return s;
#endif
}

std::ostream* Log::s_ostream = nullptr;
unsigned int Log::s_verboseLevel = 0;
bool Log::s_silent = false;

void Log::setNoOutput()
{
  static NullStreambuf nullBuf;
  static std::ostream nullStream(&nullBuf);
  s_ostream = &nullStream;
  s_silent = true;
}

} // namespace infomap
