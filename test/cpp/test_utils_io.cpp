#include "vendor/doctest.h"

#include "io/SafeFile.h"
#include "utils/FileURI.h"
#include "utils/convert.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

using infomap::FileURI;

class TempDir {
public:
  TempDir()
  {
    std::array<char, 64> pathTemplate {};
    const auto prefix = std::string("/tmp/infomap-tests-XXXXXX");
    std::copy(prefix.begin(), prefix.end(), pathTemplate.begin());
    pathTemplate[prefix.size()] = '\0';
    auto* dir = mkdtemp(pathTemplate.data());
    REQUIRE_MESSAGE(dir != nullptr, std::strerror(errno));
    m_path = dir;
  }

  ~TempDir()
  {
    rmdir(m_path.c_str());
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
