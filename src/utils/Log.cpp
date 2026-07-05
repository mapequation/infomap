/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Log.h"
#include "format.h"
#ifndef INFOMAP_R
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

#ifndef INFOMAP_R
std::ostream& Log::defaultStream()
{
  static std::ostream& s = std::cout;
  return s;
}
#endif

void Log::setNoOutput()
{
  static NullStreambuf nullBuf;
  static std::ostream nullStream(&nullBuf);
  s_ostream = &nullStream;
  s_silent = true;
}

bool Log::isWritingToStdout()
{
#ifdef INFOMAP_R
  // The R build always routes Log through the Rprintf bridge (s_ostream), never
  // the process stdout, and does not even include <iostream>.
  return false;
#else
  return &ostream() == &std::cout;
#endif
}

void Log::vprint(fmt::string_view format, fmt::format_args args)
{
  // Called only from print() under m_visible; render here so fmt/format.h stays
  // out of Log.h. Routes through the same m_ostream sink as operator<<, so the
  // R/Python redirect and verbosity/silent gating all still apply.
  m_ostream << fmt::vformat(format, args);
}

} // namespace infomap
