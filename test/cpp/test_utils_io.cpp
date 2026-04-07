#include "vendor/doctest.h"

#include "io/SafeFile.h"
#include "utils/FileURI.h"
#include "utils/convert.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

using infomap::FileURI;

int createDirectory(const char* path)
{
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path, 0700);
#endif
}

int removeDirectory(const char* path)
{
#ifdef _WIN32
  return _rmdir(path);
#else
  return rmdir(path);
#endif
}

std::string tempDirectoryRoot()
{
  for (const auto* name : { "TMPDIR", "TEMP", "TMP" }) {
    const auto* value = std::getenv(name);
    if (value != nullptr && value[0] != '\0')
      return value;
  }
#ifdef _WIN32
  return ".";
#else
  return "/tmp";
#endif
}

std::string joinPath(const std::string& directory, const std::string& filename)
{
#ifdef _WIN32
  constexpr char separator = '\\';
#else
  constexpr char separator = '/';
#endif
  if (!directory.empty() && (directory.back() == '/' || directory.back() == '\\'))
    return directory + filename;
  return directory + separator + filename;
}

class TempDir {
public:
  TempDir()
  {
    const auto root = tempDirectoryRoot();
    const auto timestamp = static_cast<unsigned long long>(std::time(nullptr));
    const auto clockTicks = static_cast<unsigned long long>(std::clock());
    int lastError = 0;

    for (unsigned int attempt = 0; attempt < 128; ++attempt) {
      const auto candidate = joinPath(root,
          "infomap-tests-" + infomap::io::stringify(timestamp) + "-" +
              infomap::io::stringify(clockTicks) + "-" + infomap::io::stringify(attempt));
      errno = 0;
      if (createDirectory(candidate.c_str()) == 0) {
        m_path = candidate;
        return;
      }
      lastError = errno;
      if (lastError != EEXIST)
        break;
    }

    REQUIRE_MESSAGE(false, std::strerror(lastError));
  }

  ~TempDir()
  {
    removeDirectory(m_path.c_str());
  }

  const std::string& path() const { return m_path; }

private:
  std::string m_path;
};

TEST_CASE("convert parses unsigned integer values strictly [fast][core][utils][io]")
{
  unsigned int value = 0;
  CHECK(infomap::io::stringToValue("42", value));
  CHECK(value == 42u);

  CHECK_FALSE(infomap::io::stringToValue("", value));
  CHECK_FALSE(infomap::io::stringToValue("-1", value));
  CHECK_FALSE(infomap::io::stringToValue("12abc", value));
  CHECK_FALSE(infomap::io::stringToValue("4294967296", value));
}

TEST_CASE("convert parses unsigned long values strictly [fast][core][utils][io]")
{
  unsigned long value = 0;
  CHECK(infomap::io::stringToValue("42", value));
  CHECK(value == 42ul);

  CHECK_FALSE(infomap::io::stringToValue("", value));
  CHECK_FALSE(infomap::io::stringToValue("-1", value));
  CHECK_FALSE(infomap::io::stringToValue("12abc", value));

  const auto overflow = infomap::io::stringify(std::numeric_limits<unsigned long long>::max()) + "0";
  CHECK_FALSE(infomap::io::stringToValue(overflow, value));
}

TEST_CASE("convert helpers preserve current tokenization behavior [fast][core][utils][io]")
{
  const auto parts = infomap::io::split("alpha,,beta,gamma", ',');
  CHECK(parts == std::vector<std::string> { "alpha", "beta", "gamma" });
  CHECK(infomap::io::firstWord("  one two three") == "one");
  CHECK(infomap::io::stringify(parts, "|") == "alpha|beta|gamma");
  CHECK(infomap::io::tolower("MiXeD") == "mixed");
  CHECK(infomap::io::InsensitiveCompare {}("abc", "ABD"));
}

TEST_CASE("FileURI splits valid paths and rejects invalid ones [fast][core][utils][io]")
{
  const FileURI nested("dir/subdir/file.net");
  CHECK(nested.getFilename() == "dir/subdir/file.net");
  CHECK(nested.getName() == "file");
  CHECK(nested.getExtension() == "net");

  const FileURI noExt("plain-file");
  CHECK(noExt.getName() == "plain-file");
  CHECK(noExt.getExtension().empty());

  CHECK_THROWS_AS(FileURI("dir/"), std::invalid_argument);
  CHECK_THROWS_AS(FileURI("file.", true), std::invalid_argument);
}

TEST_CASE("SafeFile detects writable and missing directories [fast][core][utils][io]")
{
  const TempDir tempDir;
  CHECK(infomap::isDirectoryWritable(tempDir.path() + "/"));
  CHECK_FALSE(infomap::isDirectoryWritable(tempDir.path() + "/missing/"));
}

} // namespace
