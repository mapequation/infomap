/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Log.h"
#include <iostream>
#include <streambuf>

namespace infomap {

namespace {

class NullStreambuf : public std::streambuf {
protected:
  int overflow(int c) override { return c; }
};

NullStreambuf s_nullBuf;
std::ostream s_nullStream(&s_nullBuf);

} // namespace

std::ostream* Log::s_ostream = &std::cout;
unsigned int Log::s_verboseLevel = 0;
bool Log::s_silent = false;

void Log::setNoOutput()
{
  s_ostream = &s_nullStream;
  s_silent = true;
}

} // namespace infomap
