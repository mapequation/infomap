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


#ifndef FILEURI_H_
#define FILEURI_H_
#include <iostream>
#include <string>

namespace infomap {

/**
 * Filename class to simplify handling of parts of a filename.
 * If a path is path/to/file.ext, the member methods of this class give these parts:
 * getFilename -> "path/to/file.ext"
 * getDirectory -> "path/to/"
 * getName -> "file"
 * getExtension -> "ext"
 * Can throw std::invalid_argument on creation.
 */
class FileURI {
public:
  FileURI() = default; // Allow FileURI member without initialization

  explicit FileURI(std::string filename, bool requireExtension = false);

  const std::string& getFilename() const { return m_filename; }

  /**
   * Includes last '/' if non-empty.
   */
  const std::string& getDirectory() const { return m_directory; }

  const std::string& getName() const { return m_name; }

  const std::string& getExtension() const { return m_extension; }

  std::string getParts() const
  {
    return "['" + getDirectory() + "' + '" + getName() + "' + '" + getExtension() + "']";
  }

  friend std::ostream& operator<<(std::ostream& out, const FileURI& file)
  {
    return out << file.getFilename();
  }

private:
  std::string m_filename;
  bool m_requireExtension = false;

  std::string m_directory;
  std::string m_name;
  std::string m_extension;
};

} // namespace infomap

#endif /* FILEURI_H_ */
