/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef SAFEFILE_H_
#define SAFEFILE_H_

#include <iostream>
#include <fstream>
#include <ios>
#include <cstdio>
#include "convert.h"
#include "../utils/exceptions.h"

namespace infomap {

using std::ifstream;
using std::ofstream;


/**
 * A wrapper for the C++ file stream class that automatically closes
 * the file stream when the destructor is called. Allocate it on the
 * stack to have it automatically closed when going out of scope.
 *
 * Note:
 * In C++, the only code that can be guaranteed to be executed after an
 * exception is thrown are the destructors of objects residing on the stack.
 *
 * You can exploit that fact to avoid resource leaks by tying all resources
 * to the lifespan of an object allocated on the stack. This technique is
 * called Resource Acquisition Is Initialization (RAII).
 *
 */
class SafeInFile : public ifstream {
public:
  SafeInFile(std::string filename, ios_base::openmode mode = ios_base::in)
      : ifstream(filename.c_str(), mode)
  {
    if (fail())
      throw FileOpenError(io::Str() << "Error opening file '" << filename << "'. Check that the path points to a file and that you have read permissions.");
  }

  ~SafeInFile()
  {
    if (is_open())
      close();
  }
};

class SafeOutFile : public ofstream {
public:
  SafeOutFile(std::string filename, ios_base::openmode mode = ios_base::out)
      : ofstream(filename.c_str(), mode)
  {
    if (fail())
      throw FileOpenError(io::Str() << "Error opening file '" << filename << "'. Check that the directory you are writing to exists and that you have write permissions.");
  }

  ~SafeOutFile()
  {
    if (is_open())
      close();
  }
};

inline bool isDirectoryWritable(const std::string& dir)
{
  std::string path = io::Str() << dir << "_1nf0m4p_.tmp";
  bool ok = true;
  try {
    SafeOutFile out(path.c_str());
  } catch (const FileOpenError&) {
    ok = false;
  }
  if (ok)
    std::remove(path.c_str());
  return ok;
}

} // namespace infomap

#endif /* SAFEFILE_H_ */
