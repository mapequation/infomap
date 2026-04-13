/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <sstream>

namespace infomap {

unsigned int Log::s_verboseLevel = 0;
bool Log::s_silent = false;

std::string Log::escapeJsonString(const std::string& value)
{
  std::ostringstream out;
  for (unsigned char c : value) {
    switch (c) {
    case '"':
      out << "\\\"";
      break;
    case '\\':
      out << "\\\\";
      break;
    case '\b':
      out << "\\b";
      break;
    case '\f':
      out << "\\f";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (c < 0x20) {
        out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' ');
      } else {
        out << static_cast<char>(c);
      }
      break;
    }
  }
  return out.str();
}

void Log::emitStructuredEvent(const std::string& type, const std::string& payload)
{
#ifdef __EMSCRIPTEN__
  EM_ASM(
      {
        if (typeof globalThis.__infomapEmitStructuredEvent === "function") {
          globalThis.__infomapEmitStructuredEvent(UTF8ToString($0), UTF8ToString($1));
        }
      },
      type.c_str(),
      payload.c_str());
#else
  (void)type;
  (void)payload;
#endif
}

} // namespace infomap
