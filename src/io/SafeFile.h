/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#ifndef SAFEFILE_H_
#define SAFEFILE_H_

#include "InfomapError.h"
#include "../utils/format.h"
#include <atomic>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

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
  SafeInFile(const std::string& filename, ios_base::openmode mode = ios_base::in)
      : ifstream(filename, mode)
  {
    if (fail())
      throw std::runtime_error(fmt::format(FMT_STRING("Cannot open file '{}'. Check that the path points to a file and that you have read permissions."), filename));
  }

  ~SafeInFile() override
  {
    if (is_open())
      close();
  }
};

inline bool pathExists(const std::string& path)
{
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

inline void ensureDirectoryExists(const std::string& directory)
{
  if (directory.empty())
    return;

  // create_directories creates any missing parents and handles trailing
  // separators and drive/UNC roots, so the previous manual recursion plus the
  // Windows-root special case collapse into this single call. It returns false
  // (with ec unset) when the directory already existed, so only a real error
  // that also left the path a non-directory is fatal.
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  std::error_code statusEc;
  if (ec && !std::filesystem::is_directory(directory, statusEc)) {
    throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Can't create output directory '{}'."), directory));
  }
}

inline std::string makeTemporaryOutputFilename(const std::string& filename)
{
  static std::atomic<unsigned int> counter { 0 };
#ifdef _WIN32
  const auto processId = static_cast<unsigned int>(_getpid());
#else
  const auto processId = static_cast<unsigned int>(getpid());
#endif
  return fmt::format(FMT_STRING("{}.tmp.{}.{}.{}"), filename, processId, static_cast<unsigned long long>(std::time(nullptr)), counter.fetch_add(1));
}

class SafeOutFile : public ofstream {
public:
  SafeOutFile(const std::string& filename, ios_base::openmode mode = ios_base::out, bool overwrite = true)
      : m_filename(filename),
        m_temporaryFilename(makeTemporaryOutputFilename(filename)),
        m_overwrite(overwrite)
  {
    if (!m_overwrite && pathExists(m_filename)) {
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Output file already exists: '{}'"), m_filename));
    }

    open(m_temporaryFilename, mode);
    if (fail())
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Cannot open file '{}'. Check that the directory you are writing to exists and that you have write permissions."), filename));
  }

  ~SafeOutFile() override
  {
    if (is_open())
      close();
    if (!m_committed)
      std::remove(m_temporaryFilename.c_str());
  }

  void commit()
  {
    if (m_committed)
      return;

    flush();
    if (fail()) {
      close();
      std::remove(m_temporaryFilename.c_str());
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Cannot write file '{}'."), m_filename));
    }

    close();
    if (fail()) {
      std::remove(m_temporaryFilename.c_str());
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Cannot close file '{}'."), m_filename));
    }

    if (!m_overwrite && pathExists(m_filename)) {
      std::remove(m_temporaryFilename.c_str());
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Output file already exists: '{}'"), m_filename));
    }

#ifdef _WIN32
    if (m_overwrite && pathExists(m_filename)) {
      std::remove(m_filename.c_str());
    }
#endif

    if (std::rename(m_temporaryFilename.c_str(), m_filename.c_str()) != 0) {
      std::remove(m_temporaryFilename.c_str());
      throw InfomapError(ExitCode::OutputError, fmt::format(FMT_STRING("Cannot replace file '{}'."), m_filename));
    }

    m_committed = true;
  }

private:
  std::string m_filename;
  std::string m_temporaryFilename;
  bool m_overwrite = true;
  bool m_committed = false;
};

inline bool isDirectoryWritable(const std::string& dir)
{
  std::string path = fmt::format(FMT_STRING("{}_1nf0m4p_.tmp"), dir);
  bool ok = true;
  try {
    SafeOutFile out(path);
  } catch (const std::runtime_error&) {
    ok = false;
  }
  if (ok)
    std::remove(path.c_str());
  return ok;
}

} // namespace infomap

#endif // SAFEFILE_H_
