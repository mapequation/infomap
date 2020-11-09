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


#include "FileURI.h"
#include "../io/convert.h"
#include <stdexcept>
#include <utility>

using std::string;

namespace infomap {

FileURI::FileURI(string filename, bool requireExtension)
    : m_filename(std::move(filename)), m_requireExtension(requireExtension)
{
  auto getErrorMessage = [](const auto& filename, auto requireExt) {
    string s = io::Str() << "Filename '" << filename << "' must match the pattern \"[dir/]name" << (requireExt ? ".extension\"" : "[.extension]\"");
    return s;
  };

  auto name = m_filename;
  auto pos = m_filename.find_last_of('/');
  if (pos != string::npos) {
    if (pos == m_filename.length()) // File could not end with slash
      throw std::invalid_argument(getErrorMessage(m_filename, m_requireExtension));
    m_directory = m_filename.substr(0, pos + 1); // Include the last slash in the directory
    name = m_filename.substr(pos + 1); // No slash in the name
  } else {
    m_directory = "";
  }

  pos = name.find_last_of('.');
  if (pos == string::npos || pos == 0 || pos == name.length() - 1) {
    if (pos != string::npos || m_requireExtension)
      throw std::invalid_argument(getErrorMessage(m_filename, m_requireExtension));
    m_name = name;
    m_extension = "";
  } else {
    m_name = name.substr(0, pos);
    m_extension = name.substr(pos + 1);
  }
}

} // namespace infomap
